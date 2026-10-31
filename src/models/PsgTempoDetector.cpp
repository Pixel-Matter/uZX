#include "PsgTempoDetector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

#include "EditUtilities.h"
#include "PsgClip.h"
#include "PsgFrameRetimer.h"

namespace MoTool::PsgTempo {

namespace {

constexpr int minOnsetJump = 4;             // volume step (0..15) that counts as a note start
constexpr double minGridShare = 0.8;        // chance-corrected share of onset energy on the row grid
constexpr double harmonicTolerance = 0.02;  // a doubled period may lose this much share and still win
constexpr double minEnergy = 600.0;         // about 40 full-volume hits; below that the tune is too sparse
constexpr int minSpeed = 2;
constexpr int maxSpeed = 16;
constexpr double minBpm = 80.0;  // rows per beat is halved or doubled to land in this range
constexpr double maxBpm = 170.0;
constexpr double minMeterMargin = 0.1;    // one bar length must explain repetition this much better
constexpr double minOpeningOnGrid = 0.8;  // share of early note starts that must sit on row starts
constexpr int minOpeningOnsets = 4;       // fewer early note starts than this: leave the clip where it is

/** Per-frame note-start energy: volume jumps with new-note evidence (the channel
    was silent, its tone period changed, or the envelope shape was rewritten) plus
    envelope retriggers. Jumps inside a sounding note (sample attack stages, tremolo)
    are ignored. */
std::vector<double> onsetEnergy(const uZX::PsgData& data) {
    std::vector<double> energy(data.frames.size(), 0.0);
    uZX::PsgRegsFrame state, prev;

    for (size_t i = 0; i < data.frames.size(); ++i) {
        const auto& delta = data.frames[i];
        prev = state;
        state.update(delta);
        const bool retrigger = delta.isSet(uZX::PsgRegType::EnvelopeShape);
        bool envelopeOn = false;

        for (size_t ch = 0; ch < 3; ++ch) {
            const auto volReg = state.getRaw(uZX::PsgRegType::VolumeA + ch);
            const int vol = volReg & 0x0f;
            const int prevVol = prev.getRaw(uZX::PsgRegType::VolumeA + ch) & 0x0f;
            envelopeOn = envelopeOn || (volReg & 0x10) != 0;
            const bool evidence = prevVol == 0 || state.getTonePeriod(ch) != prev.getTonePeriod(ch) || retrigger;
            if (vol - prevVol >= minOnsetJump && evidence)
                energy[i] += vol - prevVol;
        }

        if (retrigger && envelopeOn)
            energy[i] += 15.0;  // envelope-driven channels keep volume 0 in the register
    }
    return energy;
}

struct GridFit
{
    double share;
    std::vector<int> phases;
};

/** Share of onset energy on the best numPhases residues mod period, corrected for
    what random off-grid energy would land there by chance. */
GridFit gridShare(const std::vector<double>& energy, double total, int period, int numPhases) {
    std::vector<double> byPhase((size_t) period, 0.0);
    for (size_t i = 0; i < energy.size(); ++i)
        byPhase[i % (size_t) period] += energy[i];

    std::vector<int> order((size_t) period);
    std::iota(order.begin(), order.end(), 0);
    std::partial_sort(order.begin(), order.begin() + numPhases, order.end(), [&](int a, int b) {
        return byPhase[(size_t) a] > byPhase[(size_t) b];
    });

    double raw = 0.0;
    for (int k = 0; k < numPhases; ++k)
        raw += byPhase[(size_t) order[(size_t) k]];
    raw /= total;

    const double chance = (double) numPhases / period;
    std::vector<int> phases(order.begin(), order.begin() + numPhases);
    std::sort(phases.begin(), phases.end());
    return {(raw - chance) / (1.0 - chance), phases};
}

/** The row grid: row lengths in frames, repeating (one length for a uniform
    speed, two for a swing, four for a groove), starting at frame `phase`. */
struct Speed
{
    std::vector<int> rows;
    int phase;
    double share;

    int period() const { return std::accumulate(rows.begin(), rows.end(), 0); }
    double meanRow() const { return (double) period() / (double) rows.size(); }
};

/** The repeating row lengths that the best numRows residues mod period imply,
    starting at the earliest residue, or nothing when they are implausible: a
    row outside the speed range, rows more than 2 frames apart, or a pattern a
    shorter one already describes (all equal, or two halves alike). */
juce::Optional<Speed> patternFrom(const GridFit& fit, int period) {
    const auto n = fit.phases.size();
    std::vector<int> rows;
    for (size_t k = 0; k < n; ++k)
        rows.push_back((k + 1 < n ? fit.phases[k + 1] : fit.phases[0] + period) - fit.phases[k]);
    const auto [shortest, longest] = std::minmax_element(rows.begin(), rows.end());
    if (*shortest < minSpeed || *longest > maxSpeed || *longest - *shortest > 2 || *longest == *shortest)
        return {};
    if (n == 4 && rows[0] == rows[2] && rows[1] == rows[3])
        return {};
    return Speed{rows, fit.phases[0], fit.share};
}

/** Uniform rows: every divisor of the true speed also holds, so the largest
    period wins, but a multiple only beats its divisors if it keeps (almost) all
    of their energy; otherwise the tune just has most of its notes on even rows.
    Swings (two row lengths, 6-7) and grooves (four, 4-4-3-3) compete with that
    on the same terms: of the grids that fit about as well as the best one, the
    coarsest wins, since a fine uniform grid fits most patterns roughly. A swing
    or groove finer than the uniform rows only describes stray note starts inside
    rows, so it never replaces them. */
juce::Optional<Speed> detectSpeed(const std::vector<double>& energy, double total) {
    std::array<double, maxSpeed + 1> share{};
    std::vector<Speed> candidates;

    juce::Optional<Speed> uniform;
    for (int p = minSpeed; p <= maxSpeed; ++p) {
        const auto fit = gridShare(energy, total, p, 1);
        share[(size_t) p] = fit.share;
        double divisors = 0.0;
        for (int d = minSpeed; d < p; ++d)
            if (p % d == 0)
                divisors = std::max(divisors, share[(size_t) d]);
        if (fit.share >= minGridShare && fit.share >= divisors - harmonicTolerance)
            uniform = Speed{{p}, fit.phases[0], fit.share};
    }
    if (uniform)
        candidates.push_back(*uniform);

    for (int numRows : {2, 4}) {
        juce::Optional<Speed> patterned;
        for (int period = numRows * minSpeed + 1; period <= numRows * maxSpeed; ++period) {
            const auto candidate = patternFrom(gridShare(energy, total, period, numRows), period);
            if (candidate && candidate->share >= minGridShare && (!patterned || candidate->share > patterned->share))
                patterned = candidate;
        }
        // finer than the uniform rows: stray note starts inside rows, not the row grid
        if (patterned && (!uniform || patterned->meanRow() > uniform->meanRow()))
            candidates.push_back(*patterned);
    }
    if (candidates.empty())
        return {};

    double bestShare = 0.0;
    for (const auto& c : candidates)
        bestShare = std::max(bestShare, c.share);
    const Speed* chosen = nullptr;
    for (const auto& c : candidates) {
        if (c.share < bestShare - harmonicTolerance)
            continue;
        const bool coarser = chosen == nullptr || c.meanRow() > chosen->meanRow()
                             || (c.meanRow() == chosen->meanRow() && c.rows.size() < chosen->rows.size());
        if (coarser)
            chosen = &c;
    }
    return *chosen;
}

/** Frame index of every row start. */
std::vector<size_t> rowStarts(size_t numFrames, const Speed& speed) {
    std::vector<size_t> starts;
    for (size_t i = (size_t) speed.phase, k = 0; i < numFrames; i += (size_t) speed.rows[k++ % speed.rows.size()])
        starts.push_back(i);
    return starts;
}

/** What a row sounds like at its start, coarse enough to repeat bar after bar:
    the drums (channels playing noise, the noise period, an envelope retrigger
    within the row) and the bass (lowest tone, in semitones). */
struct RowFeature
{
    std::array<bool, 3> noiseChannels{};
    int noisePeriod = -1;
    bool retrigger = false;
    int bassSemitone = -1;

    bool drumsEqual(const RowFeature& o) const {
        return noiseChannels == o.noiseChannels && noisePeriod == o.noisePeriod && retrigger == o.retrigger;
    }
};

std::vector<RowFeature> rowFeatures(const uZX::PsgData& data, const std::vector<size_t>& starts) {
    std::vector<RowFeature> rows(starts.size());
    uZX::PsgRegsFrame state;
    size_t row = 0;

    for (size_t i = 0; i < data.frames.size() && row < starts.size(); ++i) {
        state.update(data.frames[i]);
        if (row + 1 < starts.size() && i >= starts[row + 1])
            ++row;
        auto& feature = rows[row];
        feature.retrigger = feature.retrigger || data.frames[i].isSet(uZX::PsgRegType::EnvelopeShape);
        if (i != starts[row])
            continue;

        const auto mixer = state.getMixer();
        int bassPeriod = 0;
        for (size_t ch = 0; ch < 3; ++ch) {
            const bool audible = (state.getRaw(uZX::PsgRegType::VolumeA + ch) & 0x1f) != 0;
            feature.noiseChannels[ch] = audible && ((mixer >> (3 + ch)) & 1) == 0;
            if (audible && ((mixer >> ch) & 1) == 0)
                bassPeriod = std::max<int>(bassPeriod, state.getTonePeriod(ch));
        }
        if (feature.noiseChannels[0] || feature.noiseChannels[1] || feature.noiseChannels[2])
            feature.noisePeriod = state.getNoisePeriod();
        if (bassPeriod > 0)
            feature.bassSemitone = juce::roundToInt(12.0 * std::log2((double) bassPeriod));
    }
    return rows;
}

/** Share of rows that sound like the row lagRows earlier, drums and bass
    counted separately. */
double repetition(const std::vector<RowFeature>& rows, size_t lagRows) {
    if (lagRows == 0 || lagRows >= rows.size())
        return 0.0;
    int matches = 0;
    for (size_t i = lagRows; i < rows.size(); ++i)
        matches += (rows[i].drumsEqual(rows[i - lagRows]) ? 1 : 0)
                   + (rows[i].bassSemitone == rows[i - lagRows].bassSemitone ? 1 : 0);
    return matches / (2.0 * (double) (rows.size() - lagRows));
}

/** Picks how many rows make a beat. Candidates group rows by 3, 4 or 5, each
    halved or doubled to land inside the BPM range; odd groups are never halved,
    so a slow triplet beat may stay below it. The winner is the beat whose half
    bars, bars and two-bar phrases (2, 4 and 8 beats) repeat best: patterns
    repeat where the music's beats start, so a wrong beat length lines up rows
    from different parts of the bar. A bar can repeat the same as 4 beats of 3
    rows or 3 beats of 4 (a waltz), so a beat other than the 4-row one also needs
    its accents (note starts per row) to recur at its own length. A swing pair or
    groove never splits across beats, so those tunes always group by 4. */
int detectRowsPerBeat(const std::vector<RowFeature>& rows,
                      const std::vector<double>& accents,
                      const Speed& speed,
                      double dataFps) {
    const bool swing = speed.rows.size() > 1;
    const double rowLength = speed.meanRow();
    auto fitToBpmRange = [&](int rowsPerBeat) {
        const int minRowsPerBeat = (int) speed.rows.size();
        double bpm = dataFps * 60.0 / (rowLength * rowsPerBeat);
        while (bpm < minBpm && rowsPerBeat % 2 == 0 && rowsPerBeat / 2 >= minRowsPerBeat) {
            rowsPerBeat /= 2;
            bpm *= 2.0;
        }
        while (bpm > maxBpm) {
            rowsPerBeat *= 2;
            bpm /= 2.0;
        }
        return rowsPerBeat;
    };

    const double mean =
        accents.empty() ? 0.0 : std::accumulate(accents.begin(), accents.end(), 0.0) / (double) accents.size();
    auto accentRecurrence = [&](int lag) {
        double sum = 0.0;
        for (size_t i = (size_t) lag; i < accents.size(); ++i)
            sum += (accents[i - (size_t) lag] - mean) * (accents[i] - mean);
        return sum;
    };
    auto repeats = [&](int rowsPerBeat) {
        return (repetition(rows, (size_t) (2 * rowsPerBeat)) + repetition(rows, (size_t) (4 * rowsPerBeat))
                + repetition(rows, (size_t) (8 * rowsPerBeat)))
               / 3.0;
    };

    const int fourRowBeat = fitToBpmRange(4);
    if (swing)
        return fourRowBeat;
    int best = fourRowBeat;
    double bestScore = repeats(fourRowBeat);
    for (int group : {3, 5}) {
        const int beat = fitToBpmRange(group);
        const double score = repeats(beat);
        const double accent = accentRecurrence(beat);
        if (score > bestScore && accent > 0.0 && accent > accentRecurrence(fourRowBeat)) {
            best = beat;
            bestScore = score;
        }
    }
    return best;
}

/** Drum and bass patterns repeat every bar, so rows one bar apart match more
    often than rows any other distance apart. Returns how much better bars of 3
    (and 6) beats explain the repetition than bars of 4 (and 8): clearly
    positive means 3/4, clearly negative 4/4, near zero no evidence either way. */
double tripleMeterMargin(const std::vector<RowFeature>& rows, int rowsPerBeat) {
    auto bars = [&](int beats) { return repetition(rows, (size_t) (beats * rowsPerBeat)); };
    return (bars(3) + bars(6)) / 2.0 - (bars(4) + bars(8)) / 2.0;
}

/** Whether the first bars of the tune follow the detected row grid. The grid is
    fitted to the whole tune; an intro at another speed would otherwise move the
    clip by a bogus offset. Note starts on frame 0 are skipped: everything audible
    there counts as a start, whether a row begins or not. */
bool openingFollowsGrid(const std::vector<double>& energy, const Speed& speed, size_t openingFrames) {
    const auto period = (size_t) speed.period();
    std::vector<size_t> rowOffsets;
    for (size_t k = 0, offset = 0; k < speed.rows.size(); offset += (size_t) speed.rows[k++])
        rowOffsets.push_back(offset);
    int onsets = 0, onGrid = 0;
    for (size_t i = 1; i < std::min(openingFrames, energy.size()); ++i) {
        if (energy[i] <= 0.0)
            continue;
        ++onsets;
        const auto residue = (i + period - (size_t) speed.phase % period) % period;
        if (std::find(rowOffsets.begin(), rowOffsets.end(), residue) != rowOffsets.end())
            ++onGrid;
    }
    return onsets >= minOpeningOnsets && onGrid >= minOpeningOnGrid * onsets;
}

}  // namespace

std::vector<int> Result::subdivisionPattern() const {
    int groups = rowsPerBeat;
    for (int candidate : {4, 3, 5, 2})
        if (rowsPerBeat % candidate == 0) {
            groups = candidate;
            break;
        }
    const int rowsPerGroup = rowsPerBeat / groups;
    std::vector<int> pattern((size_t) groups, 0);
    for (int row = 0; row < rowsPerBeat; ++row)
        pattern[(size_t) (row / rowsPerGroup)] += rowFrames[(size_t) row % rowFrames.size()];
    return pattern;
}

juce::Optional<Result> detect(const uZX::PsgData& data) {
    const auto energy = onsetEnergy(data);
    const double total = std::accumulate(energy.begin(), energy.end(), 0.0);
    if (total < minEnergy)
        return {};

    const auto speed = detectSpeed(energy, total);
    if (!speed)
        return {};

    const auto playRate = (int) std::max<size_t>(1, data.options.playRate);  // machine frames per data frame
    Result result;
    for (int row : speed->rows)
        result.rowFrames.push_back(row * playRate);
    const auto starts = rowStarts(energy.size(), *speed);
    const auto rows = rowFeatures(data, starts);
    std::vector<double> accents;
    for (auto i : starts)
        accents.push_back(energy[i]);
    result.rowsPerBeat = detectRowsPerBeat(rows, accents, *speed, data.getFrameRate() / playRate);
    const auto meterMargin = tripleMeterMargin(rows, result.rowsPerBeat);
    result.beatsPerBar = meterMargin > minMeterMargin ? 3 : 4;
    result.meterClear = std::abs(meterMargin) > minMeterMargin;
    for (int row = 0; row < result.rowsPerBeat; ++row)
        result.framesPerBeat += result.rowFrames[(size_t) row % result.rowFrames.size()];
    result.bpm = data.getFrameRate() * 60.0 / result.framesPerBeat;
    result.gridShare = speed->share;
    const auto barFrames = (size_t) (result.framesPerBeat * result.beatsPerBar / playRate);
    const int firstRow = speed->phase;
    if (openingFollowsGrid(energy, *speed, 2 * barFrames))
        result.firstRowFrame = firstRow * playRate;

    uZX::PsgRegsFrame state;
    for (size_t i = 0; i < (size_t) firstRow && i < data.frames.size(); ++i) {
        state.update(data.frames[i]);
        for (size_t ch = 0; ch < 3; ++ch)
            if ((state.getRaw(uZX::PsgRegType::VolumeA + ch) & 0x1f) != 0)
                result.silentLeadIn = false;
    }
    return result;
}

namespace {

/** Whether the clip is alone in its edit: tempo and meter are only taken from
    the file when nothing else depends on them. */
bool isOnlyClipInEdit(PsgClip& clip) {
    for (auto* track : tracktion::getClipTracks(clip.edit)) {
        for (auto* other : track->getClips())
            if (other != &clip)
                return false;
        if (auto* audioTrack = dynamic_cast<tracktion::AudioTrack*>(track))
            for (auto* slot : audioTrack->getClipSlotList().getClipSlots())
                if (auto* other = slot->getClip(); other != nullptr && other != &clip)
                    return false;
    }
    return true;
}

}  // namespace

void applyToEdit(PsgClip& clip, const uZX::PsgData& data) {
    if (!isOnlyClipInEdit(clip))
        return;
    const auto result = detect(data);
    if (!result)
        return;

    auto& edit = clip.edit;
    const auto clipStart = clip.getPosition().getStart();
    PsgTiming::setFramesPerBeat(edit,
                                edit.tempoSequence.getTempoAt(clipStart),
                                result->framesPerBeat,
                                /*preserveAbsoluteFrameTimes=*/true);
    Helpers::setGridSubdivisionPattern(edit, result->subdivisionPattern());
    auto& timeSig = edit.tempoSequence.getTimeSigAt(clipStart);
    if (result->meterClear)
        timeSig.numerator = result->beatsPerBar;

    if (result->firstRowFrame <= 0)
        return;

    const auto fps = Helpers::getProjectFps(edit);
    auto position = clip.getPosition();
    if (result->silentLeadIn) {
        const auto lead = tracktion::TimeDuration::fromSeconds(result->firstRowFrame / fps);
        position.time = {position.getStart(), position.getEnd() - lead};
        position.offset = position.offset + lead;
        clip.setPosition(position);
    } else {
        const int barFrames = result->framesPerBeat * timeSig.numerator.get();
        const int shift = barFrames - result->firstRowFrame % barFrames;
        clip.setStart(position.getStart() + tracktion::TimeDuration::fromSeconds(shift / fps), false, true);
    }
}

}  // namespace MoTool::PsgTempo
