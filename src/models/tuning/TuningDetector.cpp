#include "TuningDetector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace MoTool {

namespace {

constexpr int firstGridNote = 24;  // C1, where every registry table starts
constexpr int numGridNotes = 96;   // eight octaves

// Krumhansl-Kessler probe-tone profiles for a major and a minor key on C.
constexpr std::array<float, 12>
    kkMajor{6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
constexpr std::array<float, 12>
    kkMinor{6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};

template <typename Xs, typename Ys>
float pearson(const Xs& xs, const Ys& ys) {
    const auto n = static_cast<float>(xs.size());
    const float mx = std::accumulate(xs.begin(), xs.end(), 0.0f) / n;
    const float my = std::accumulate(ys.begin(), ys.end(), 0.0f) / n;
    float cov = 0.0f, vx = 0.0f, vy = 0.0f;
    for (size_t i = 0; i < xs.size(); ++i) {
        const float dx = xs[i] - mx, dy = ys[i] - my;
        cov += dx * dy;
        vx += dx * dx;
        vy += dy * dy;
    }
    const float var = std::sqrt(vx) * std::sqrt(vy);
    return var > 0.0f ? cov / var : 0.0f;
}

/** Grid as (log2 period, note) pairs sorted by descending period, for off-grid lookups. */
struct LogGrid
{
    std::vector<std::pair<double, int>> points;

    explicit LogGrid(const PeriodGrid& grid) {
        points.reserve(grid.size());
        for (auto it = grid.rbegin(); it != grid.rend(); ++it)
            points.emplace_back(std::log2(static_cast<double>(it->first)), it->second);
    }

    /** Fractional MIDI note of a period against the nearest grid note. */
    double noteOf(double period) const {
        const double lp = std::log2(period);
        auto it = std::lower_bound(
            points.begin(), points.end(), lp, [](const auto& point, double value) { return point.first > value; });
        auto best = it;
        if (it == points.end()
            || (it != points.begin() && std::abs(std::prev(it)->first - lp) < std::abs(it->first - lp)))
            best = std::prev(it);
        return best->second + (best->first - lp) * 12.0;
    }
};

/** Clock / 16 / frequency of every grid note at A4 = 1 Hz: an equal-temperament period is
    this divided by A4, so a sweep needs no pow() per step. */
const std::array<double, numGridNotes>& equalTemperamentPeriodsAtUnitA4() {
    static const auto periods = [] {
        std::array<double, numGridNotes> p{};
        for (int i = 0; i < numGridNotes; ++i)
            p[static_cast<size_t>(i)] = tuningGridClock / 16.0 / std::pow(2.0, (firstGridNote + i - 69) / 12.0);
        return p;
    }();
    return periods;
}

/** Fills `out` with the equal-temperament periods for a4, ascending. */
void equalTemperamentPeriods(double a4, std::vector<int>& out) {
    const auto& unit = equalTemperamentPeriodsAtUnitA4();
    out.resize(unit.size());
    // Periods fall as notes rise, so filling from the top note gives ascending order.
    for (size_t i = 0; i < unit.size(); ++i)
        out[i] = static_cast<int>(std::round(unit[unit.size() - 1 - i] / a4));
}

/** A histogram as flat (period, frames) lists, with the envelope pitches that cannot tell
    grids apart already dropped: the 12TET sweep scores it a few hundred times. */
struct FlatHistogram
{
    std::vector<std::pair<int, int>> tone, envelope;
    int total = 0;

    explicit FlatHistogram(const PitchHistogram& hist) {
        tone.assign(hist.tone.begin(), hist.tone.end());
        for (const auto& [period, frames] : hist.envelope)
            if (PitchHistogram::isEnvelopeDiscriminating(period))
                envelope.emplace_back(period, frames);
        for (const auto* part : {&tone, &envelope})
            for (const auto& [period, frames] : *part)
                total += frames;
    }

    /** Both lists are ascending, so one merge walk per grid scores it in linear time. */
    float shareOn(const std::vector<int>& ascendingPeriods) const {
        if (total == 0)
            return 0.0f;
        int hits = 0;
        const auto gridEnd = ascendingPeriods.end();
        auto g = ascendingPeriods.begin();
        for (const auto& [period, frames] : tone) {
            while (g != gridEnd && *g < period)
                ++g;
            if (g != gridEnd && *g == period)
                hits += frames;
        }
        g = ascendingPeriods.begin();
        for (const auto& [period, frames] : envelope) {
            while (g != gridEnd && *g < period - PitchHistogram::envelopeTolerance)
                ++g;
            if (g != gridEnd && *g <= period + PitchHistogram::envelopeTolerance)
                hits += frames;
        }
        return static_cast<float>(hits) / static_cast<float>(total);
    }
};

std::vector<int> periodsOf(const PeriodGrid& grid) {
    std::vector<int> periods;
    periods.reserve(grid.size());
    for (const auto& [period, note] : grid)
        periods.push_back(period);
    return periods;
}

/** Jaccard overlap of two grids' period sets. */
float gridOverlap(const PeriodGrid& a, const PeriodGrid& b) {
    int common = 0;
    for (const auto& [period, note] : a)
        common += b.count(period) > 0 ? 1 : 0;
    const auto unionSize = static_cast<int>(a.size() + b.size()) - common;
    return unionSize > 0 ? static_cast<float>(common) / static_cast<float>(unionSize) : 0.0f;
}

}  // namespace

//==============================================================================
PeriodGrid makePeriodGrid(const TuningSystem& tuning) {
    PeriodGrid grid;
    for (int note = firstGridNote; note < firstGridNote + numGridNotes; ++note)
        grid.try_emplace(tuning.midiNoteToPeriod(note), note);
    return grid;
}

PeriodGrid makeEqualTemperamentGrid(double a4Frequency, double chipClock) {
    jassert(a4Frequency > 0.0 && chipClock > 0.0);
    PeriodGrid grid;
    for (int note = firstGridNote; note < firstGridNote + numGridNotes; ++note) {
        const double frequency = a4Frequency * std::pow(2.0, (note - 69) / 12.0);
        grid.try_emplace(static_cast<int>(std::round(chipClock / 16.0 / frequency)), note);
    }
    return grid;
}

const PeriodGrid& getBuiltinPeriodGrid(BuiltinTuningType table) {
    static const auto grids = [] {
        std::vector<PeriodGrid> all;
        for (int i = 0; i < static_cast<int>(BuiltinTuningType::size()); ++i)
            all.push_back(makePeriodGrid(*makeBuiltinTuning(BuiltinTuningType(i))));
        return all;
    }();
    return grids[static_cast<size_t>(static_cast<int>(table))];
}

String getEqualTemperamentLabel(double a4Frequency) {
    return "12TET " + String(roundToInt(a4Frequency)) + " Hz";
}

//==============================================================================
void PitchHistogram::Builder::addFrame(const PsgParamFrameData& frame, int frames) {
    jassert(frames > 0);
    visitFrameNotes(frame, [this, frames](const PsgFrameNote& note) {
        jassert(note.period >= 0 && note.period < (1 << 16));
        (note.isEnvelopePitch ? envelope_ : tone_)[static_cast<size_t>(note.period)] += frames;
    });
}

PitchHistogram PitchHistogram::Builder::build() const {
    PitchHistogram hist;
    for (int period = 0; period < static_cast<int>(tone_.size()); ++period) {
        if (const int frames = tone_[static_cast<size_t>(period)])
            hist.tone.emplace_hint(hist.tone.end(), period, frames);
        if (const int frames = envelope_[static_cast<size_t>(period)])
            hist.envelope.emplace_hint(hist.envelope.end(), period, frames);
    }
    return hist;
}

int PitchHistogram::getTotal() const noexcept {
    int total = 0;
    for (const auto& [period, frames] : tone)
        total += frames;
    for (const auto& [period, frames] : envelope)
        total += frames;
    return total;
}

bool PitchHistogram::isEnvelopeDiscriminating(int period) noexcept {
    // The window must be narrower than the semitone spacing at this period.
    static const double semitone = std::pow(2.0, 1.0 / 12.0) - 1.0;
    return 2.0 * envelopeTolerance < period * semitone;
}

float PitchHistogram::getShareOnGrid(const PeriodGrid& grid) const {
    return getShareOnPeriods(periodsOf(grid));
}

float PitchHistogram::getShareOnPeriods(const std::vector<int>& ascendingPeriods) const {
    return FlatHistogram(*this).shareOn(ascendingPeriods);
}

//==============================================================================
String DetectedKey::getName() const {
    return String::fromUTF8(tonic.getLongLabel().data()) + (minor ? " minor" : " major");
}

Scale::ScaleType DetectedKey::getScaleType() const {
    return minor ? Scale::ScaleType::AeolianOrMinor : Scale::ScaleType::IonianOrMajor;
}

std::vector<DetectedKey> rankKeys(const std::array<float, 12>& pitchClasses) {
    std::vector<DetectedKey> keys;
    keys.reserve(24);
    for (int tonic = 0; tonic < 12; ++tonic) {
        for (bool minor : {false, true}) {
            const auto& profile = minor ? kkMinor : kkMajor;
            std::array<float, 12> rotated{};
            for (int pc = 0; pc < 12; ++pc)
                rotated[static_cast<size_t>(pc)] = profile[static_cast<size_t>((pc - tonic + 12) % 12)];
            keys.push_back({Scale::Tonic(tonic), minor, pearson(pitchClasses, rotated)});
        }
    }
    std::stable_sort(
        keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.correlation > b.correlation; });
    return keys;
}

bool isKeyTie(const std::vector<DetectedKey>& rankedKeys) {
    return rankedKeys.size() > 1 && rankedKeys[0].correlation - rankedKeys[1].correlation < TuningDetection::tieGap;
}

std::array<float, 12> makePitchClassHistogram(const PitchHistogram& hist, const PeriodGrid& grid) {
    std::array<float, 12> pcs{};
    if (grid.empty())
        return pcs;
    const LogGrid logGrid(grid);
    for (const auto* part : {&hist.tone, &hist.envelope}) {
        for (const auto& [period, frames] : *part) {
            auto it = grid.find(period);
            const int note = it != grid.end() ? it->second : roundToInt(logGrid.noteOf(period));
            pcs[static_cast<size_t>(((note % 12) + 12) % 12)] += static_cast<float>(frames);
        }
    }
    const float total = std::accumulate(pcs.begin(), pcs.end(), 0.0f);
    if (total > 0.0f)
        for (auto& pc : pcs)
            pc /= total;
    return pcs;
}

std::vector<float> makeFoldedPitchHistogram(const PitchHistogram& hist, const PeriodGrid& grid, int binsPerSemitone) {
    const int numBins = 12 * binsPerSemitone;
    std::vector<float> bins(static_cast<size_t>(numBins), 0.0f);
    if (grid.empty())
        return bins;
    const LogGrid logGrid(grid);
    auto position = [&](double period) {
        const double note = logGrid.noteOf(period);
        return std::fmod(std::fmod(note, 12.0) + 12.0, 12.0) * binsPerSemitone;
    };

    for (const auto [part, halfBand] :
         {std::pair{&hist.tone, 0.5}, std::pair{&hist.envelope, double(PitchHistogram::envelopeTolerance)}}) {
        for (const auto& [period, frames] : *part) {
            // A band of an octave or more says nothing about the pitch class.
            if (12.0 * std::log2((period + halfBand) / (period - halfBand)) >= 12.0)
                continue;
            double lo = position(period + halfBand), hi = position(period - halfBand);
            if (hi < lo)
                hi += numBins;
            const int span = jmax(1, static_cast<int>(std::ceil(hi - lo)));
            const float weight = static_cast<float>(frames) / static_cast<float>(span);
            for (int k = 0; k < span; ++k) {
                // Round the midpoint of each sub-band to its bin.
                const auto bin = static_cast<int>(std::floor(lo + (k + 0.5) * (hi - lo) / span + 0.5));
                bins[static_cast<size_t>(((bin % numBins) + numBins) % numBins)] += weight;
            }
        }
    }
    const float peak = *std::max_element(bins.begin(), bins.end());
    if (peak > 0.0f)
        for (auto& bin : bins)
            bin /= peak;
    return bins;
}

//==============================================================================
String TuningDetection::getTableLabel() const {
    if (usesEqualTemperament || tables.empty())
        return getEqualTemperamentLabel(equalTemperament.a4Frequency);
    return String(tables.front().table.getShortLabel().data());
}

/** Built-in tables with near-identical grids merged; the later registry entry names a
    group (PT #3 absorbs PT #0, the same list a semitone away). Data-independent. */
static const std::vector<BuiltinTuningType>& getCandidateTables() {
    static const auto candidates = [] {
        std::vector<BuiltinTuningType> reps;
        for (int i = 0; i < static_cast<int>(BuiltinTuningType::size()); ++i) {
            const BuiltinTuningType table(i);
            auto same = std::find_if(reps.begin(), reps.end(), [&](auto rep) {
                return gridOverlap(getBuiltinPeriodGrid(rep), getBuiltinPeriodGrid(table)) > 0.9f;
            });
            if (same != reps.end())
                *same = table;
            else
                reps.push_back(table);
        }
        return reps;
    }();
    return candidates;
}

TuningDetection detectTuning(const PitchHistogram& hist) {
    TuningDetection result;
    result.histogram = hist;
    result.keys = rankKeys({});
    result.hasNotes = hist.getTotal() > 0;
    if (!result.hasNotes)
        return result;

    const FlatHistogram flat(hist);
    for (auto table : getCandidateTables())
        result.tables.push_back({table, flat.shareOn(periodsOf(getBuiltinPeriodGrid(table)))});
    std::stable_sort(
        result.tables.begin(), result.tables.end(), [](const auto& a, const auto& b) { return a.share > b.share; });

    // Equal temperament: A4 and A4 * 2^(1/12) give the same period set, so one semitone
    // around 440 Hz is the whole search space. The middle of the plateau of equally good
    // A4 values is refined at a tenth of a step: the lowest notes move one period unit
    // per half cent, so the coarse grid can miss their exact value.
    constexpr int steps = 200;
    auto a4At = [](double step) { return 440.0 * std::pow(2.0, (step / steps - 0.5) / 12.0); };
    std::vector<int> periods;
    auto shareAt = [&](double step) {
        equalTemperamentPeriods(a4At(step), periods);
        return flat.shareOn(periods);
    };
    std::vector<std::pair<float, double>> sweep;
    sweep.reserve(steps);
    for (int k = 0; k < steps; ++k)
        sweep.emplace_back(shareAt(k), double(k));
    const float bestShare = std::max_element(sweep.begin(), sweep.end())->first;
    std::vector<double> plateau;
    for (const auto& [share, step] : sweep)
        if (share >= bestShare - 0.01f)
            plateau.push_back(step);
    double bestStep = plateau[plateau.size() / 2];
    float refined = bestShare;
    for (int k = -10; k <= 10; ++k) {
        const double step = plateau[plateau.size() / 2] + k / 10.0;
        const float share = shareAt(step);
        if (share > refined) {
            refined = share;
            bestStep = step;
        }
    }
    result.equalTemperament = {a4At(bestStep), refined};

    // A named table wins a draw: PT #2 is equal temperament at the Pentagon clock, so the
    // fit reaches its share exactly. The fit wins only when it is clearly better.
    const float bestTable = result.tables.front().share;
    const float secondTable = result.tables.size() > 1 ? result.tables[1].share : 0.0f;
    if (result.equalTemperament.share > bestTable + TuningDetection::tieGap) {
        result.usesEqualTemperament = true;
        result.grid = makeEqualTemperamentGrid(result.equalTemperament.a4Frequency, tuningGridClock);
        result.share = result.equalTemperament.share;
    } else {
        result.grid = getBuiltinPeriodGrid(result.tables.front().table);
        result.share = bestTable;
        result.tableTie = bestTable - secondTable < TuningDetection::tieGap;
    }

    result.pitchClasses = makePitchClassHistogram(hist, result.grid);
    result.keys = rankKeys(result.pitchClasses);
    result.keyTie = isKeyTie(result.keys);
    return result;
}

}  // namespace MoTool
