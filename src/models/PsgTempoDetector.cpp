#include "PsgTempoDetector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numeric>

#include "EditUtilities.h"
#include "PsgClip.h"
#include "PsgFrameRetimer.h"
#include "PsgParameter.h"
#include "PsgRowEvidence.h"

namespace MoTool::PsgTempo {

namespace {

using Chips = std::vector<const uZX::PsgData*>;

constexpr int minOnsetJump = 4;             // volume step (0..15) that counts as a note start
constexpr double harmonicTolerance = 0.02;  // a doubled period may lose this much share and still win
constexpr double minEnergy = 600.0;         // about 40 full-volume hits; below that the tune is too sparse
constexpr double minEvidence = 240.0;       // the same for learnt row evidence: a clear note start weighs about 6
constexpr double pulseStrength = 0.8;       // the pulse repeats this well against the lag that repeats best
constexpr double minRowStarts = 0.02;       // energy share a swing's or groove's row starts, or it starts nothing
constexpr int minSpeed = 2;
constexpr int maxSpeed = 16;
constexpr int grooveStretchBeats = 4;      // beats in each stretch that the rows inside the beat are read in
constexpr double grooveRowStrength = 0.5;  // of the strongest frame's evidence: a frame with it starts a row
constexpr double minGrooveShare = 0.5;     // the rows of a stretch hold this much of its evidence, or it shows none
constexpr double minBpm = 80.0;  // rows per beat is halved or doubled to land in this range
constexpr double maxBpm = 170.0;
// The range is read at 50 Hz, as trackers count their rows, so a tune at 200 Hz gets the same rows
// per beat and four times the BPM, up to Tracktion's tempo limit.
constexpr double bpmRangeFps = 50.0;
// minBeatFrames and maxBeatFrames are that range as frames to the beat, at that rate
static_assert((minBeatFrames - 1) * maxBpm < bpmRangeFps * 60.0 && minBeatFrames * maxBpm >= bpmRangeFps * 60.0);
static_assert(maxBeatFrames * minBpm <= bpmRangeFps * 60.0 && (maxBeatFrames + 1) * minBpm > bpmRangeFps * 60.0);
constexpr double minOpeningOnGrid = 0.8;  // share of early note starts that must sit on row starts
constexpr double minPulseShare = 0.35;    // of the evidence on a pulse's own frames, for it to place the clip
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
    std::vector<double> starts;  // share of the energy on each of the phases
};

/** Share of onset energy on the best numPhases residues mod period, corrected for
    what random off-grid energy would land there by chance. */
GridFit fitGrid(std::span<const double> energy, double total, int period, int numPhases) {
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
    std::vector<double> starts;
    for (int phase : phases)
        starts.push_back(byPhase[(size_t) phase] / total);
    return {(raw - chance) / (1.0 - chance), phases, starts};
}

/** The row grid: row lengths in frames, repeating (one length for a uniform
    speed, two for a swing, four for a groove), starting at frame `phase`. */
struct Speed
{
    std::vector<int> rows;
    int phase;
    double share;
    int beatRows = 0;  // rows to the beat when what found the rows also found the beat, else 0
    bool guessed = false;  // a module's rows follow no pattern: `rows` is only their commonest length

    int period() const { return std::accumulate(rows.begin(), rows.end(), 0); }
    double meanRow() const { return (double) period() / (double) rows.size(); }
};

/** A search for the row grid: the one chosen, if any, and those it was chosen from. */
struct GridSearch
{
    juce::Optional<Speed> chosen;
    std::vector<Speed> tried;  // every uniform speed, then the swing and the groove that fit best, if any
};

/** A search for the pulse and its beat. */
struct PulseSearch
{
    juce::Optional<Speed> chosen;
    int pulse = 0;                 // frames; 0 when nothing repeats well enough
    double grooveAgreement = 0.0;  // above 0: the chosen rows were read inside the beat, by so many of the stretches
    std::vector<Analysis::Beat> tried;
};

/** A search for the rows that make a beat. */
struct BeatSearch
{
    int rowsPerBeat;
    std::vector<Analysis::Beat> tried;
};

/** The repeating row lengths that the best numRows residues mod period imply,
    starting at the earliest residue, or nothing when they are implausible: a
    row outside the speed range, rows more than 2 frames apart, a pattern a
    shorter one already describes (all equal, or two halves alike), or plain rows
    in disguise: a row that starts next to nothing is no row, and when the rows
    left are evenly spaced the tune has those, with a stray note start between
    them (4-2-2-4 found at speed 4, where every third row had one). */
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
    std::vector<int> starting;
    for (size_t k = 0; k < n; ++k)
        if (fit.starts[k] >= minRowStarts)
            starting.push_back(fit.phases[k]);
    if (starting.size() < n) {
        bool evenlySpaced = true;
        for (size_t k = 0; k + 1 < starting.size(); ++k)
            evenlySpaced = evenlySpaced && starting[k + 1] - starting[k] == starting[0] + period - starting.back();
        if (evenlySpaced)
            return {};
    }
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
GridSearch detectSpeed(const std::vector<double>& energy, double total) {
    std::array<double, maxSpeed + 1> share{};
    std::vector<Speed> candidates;
    GridSearch search;

    juce::Optional<Speed> uniform;
    for (int p = minSpeed; p <= maxSpeed; ++p) {
        const auto fit = fitGrid(energy, total, p, 1);
        search.tried.push_back(Speed{{p}, fit.phases[0], fit.share});
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
        juce::Optional<Speed> patterned;  // the one that fits best
        for (int period = numRows * minSpeed + 1; period <= numRows * maxSpeed; ++period) {
            const auto candidate = patternFrom(fitGrid(energy, total, period, numRows), period);
            if (candidate && (!patterned || candidate->share > patterned->share))
                patterned = candidate;
        }
        if (patterned)
            search.tried.push_back(*patterned);
        // finer than the uniform rows: stray note starts inside rows, not the row grid
        if (patterned && patterned->share >= minGridShare && (!uniform || patterned->meanRow() > uniform->meanRow()))
            candidates.push_back(*patterned);
    }
    if (candidates.empty())
        return search;

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
    search.chosen = *chosen;
    return search;
}

/** How alike the energy is to itself `lag` frames on, for every lag up to maxLag: 1 at lag 0. */
std::vector<double> autocorrelation(const std::vector<double>& energy, size_t maxLag) {
    const double mean = std::accumulate(energy.begin(), energy.end(), 0.0) / (double) energy.size();
    std::vector<double> repeat(maxLag + 1, 0.0);
    for (size_t lag = 0; lag <= maxLag; ++lag)
        for (size_t i = lag; i < energy.size(); ++i)
            repeat[lag] += (energy[i] - mean) * (energy[i - lag] - mean);
    if (repeat[0] > 0.0)
        for (size_t lag = maxLag + 1; lag-- > 0;)
            repeat[lag] /= repeat[0];
    return repeat;
}

/** The beat of a pulse that is not a whole number of frames: rows of 4-4-4-4-4-4-4-3 frames pulse
    every 7.75, which reads as 8, and twice and four times 8 drift off the lags the tune repeats
    at (31, 62). So each doubling is taken at the lag within a frame of it that repeats best.
    Returns the beat inside the BPM range that repeats best over 1, 2 and 4 beats taken so, and how
    well; a beat of 0 when none is in the range. */
std::pair<int, double> driftingBeat(const std::vector<double>& repeat, int pulse) {
    const int maxLag = (int) repeat.size() - 1;
    auto twice = [&](int lag) {
        const int exact = std::min(2 * lag, maxLag);
        int best = exact;
        for (int other : {exact - 1, exact + 1})
            if (other <= maxLag && repeat[(size_t) other] > repeat[(size_t) best])
                best = other;
        return best;
    };
    std::pair<int, double> found{0, 0.0};
    for (int beat = pulse; beat <= maxBeatFrames; beat = twice(beat)) {
        const int two = twice(beat), four = twice(two);
        const double over = (repeat[(size_t) beat] + repeat[(size_t) two] + repeat[(size_t) four]) / 3.0;
        if (beat >= minBeatFrames && (found.first == 0 || over >= found.second))
            found = {beat, over};
    }
    return found;
}

/** The row lengths cut to their shortest repeating unit: 6-7-6-7 is 6-7. */
std::vector<int> shortestUnit(std::vector<int> rows) {
    for (size_t unit = 1; unit < rows.size(); ++unit)
        if (rows.size() % unit == 0 && std::equal(rows.begin() + (std::ptrdiff_t) unit, rows.end(), rows.begin())) {
            rows.resize(unit);
            break;
        }
    return rows;
}

/** The key that a map of counts holds most of; the first of them when several hold as much. */
template <typename Counts>
auto commonestOf(const Counts& counts) {
    return std::max_element(counts.begin(), counts.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });
}

/** How many frames each row lasts, from the frame every row starts at; the last row's length is not known. */
std::vector<int> rowLengths(const std::vector<size_t>& starts) {
    std::vector<int> lengths;
    for (size_t i = 1; i < starts.size(); ++i)
        lengths.push_back((int) (starts[i] - starts[i - 1]));
    return lengths;
}

/** The rows inside a beat, of a tune no row grid holds through. */
struct Groove
{
    std::vector<int> rows;   // row lengths, repeating, from the first row of the beat
    int rowsPerBeat = 0;
    int phase = 0;           // the frame the first row starts at
    double agreement = 0.0;  // share of the tune's stretches that read these rows
};

/** Reads the rows inside a beat of `beat` frames, where a grid cannot: a row a frame longer now and
    then moves every row after it, so the tune is read in stretches of a few beats, each folded at
    the beat as it stands. The frames of the beat that hold at least half the evidence of the
    strongest one start rows; their gaps, turned to their smallest rotation, are the stretch's
    groove, and the stretches vote. A swing as wide as 4-7 is read so, and rows of 4-4-4-4-4-4-4-3
    with every second one accented read as 8-8-8-7. A stretch whose rows come out one twice as
    long as another has a row without a note in it, and does not vote.

    Nothing when too few stretches agree, or when what they agree on is one row length: that is
    the pulse, which is known. The rows start where they do in most of the stretches that agree. */
juce::Optional<Groove> readGroove(const std::vector<double>& energy, int beat) {
    const auto period = (size_t) beat, length = (size_t) (grooveStretchBeats * beat);
    std::map<std::vector<int>, std::map<std::vector<int>, size_t>> votes;  // groove -> the frames its rows start at
    size_t stretches = 0;
    for (size_t start = 0; start + length <= energy.size(); start += length / 2) {
        std::vector<double> held(period, 0.0);
        for (size_t i = start; i < start + length; ++i)
            held[i % period] += energy[i];
        const double all = std::accumulate(held.begin(), held.end(), 0.0);
        if (all <= 0.0)
            continue;
        ++stretches;
        const double strongest = *std::max_element(held.begin(), held.end());
        std::vector<int> phases;
        double onRows = 0.0;
        for (size_t phase = 0; phase < period; ++phase)
            if (held[phase] >= grooveRowStrength * strongest) {
                phases.push_back((int) phase);
                onRows += held[phase];
            }
        if (onRows < minGrooveShare * all || phases.size() < 2 || 2 * phases.size() > period)
            continue;
        std::vector<int> gaps;
        for (size_t k = 0; k < phases.size(); ++k)
            gaps.push_back((k + 1 < phases.size() ? phases[k + 1] : phases[0] + beat) - phases[k]);
        // Two frames side by side are one row that slips inside the stretch, not two rows; a row twice
        // as long as another is two, one of which starts no note here: the rows were not all read
        const auto [shortest, longest] = std::minmax_element(gaps.begin(), gaps.end());
        if (*shortest < minSpeed || *longest >= 2 * *shortest)
            continue;
        auto groove = shortestUnit(gaps);
        auto smallest = groove;
        for (size_t turn = 1; turn < groove.size(); ++turn) {
            std::rotate(groove.begin(), groove.begin() + 1, groove.end());
            smallest = std::min(smallest, groove);
        }
        ++votes[smallest][phases];
    }

    auto count = [](const auto& places) {
        size_t n = 0;
        for (const auto& [phases, times] : places)
            n += times;
        return n;
    };
    const auto winner = std::max_element(votes.begin(), votes.end(), [&](const auto& a, const auto& b) {
        return count(a.second) < count(b.second);
    });
    if (winner == votes.end() || winner->first.size() < 2)
        return {};
    const double agreement = (double) count(winner->second) / (double) stretches;
    if (agreement < minGrooveAgreement)
        return {};
    // where the rows start in most of the stretches that agree
    const auto place = commonestOf(winner->second);
    const auto& phases = place->first;
    std::vector<int> rows;
    for (size_t k = 0; k < phases.size(); ++k)
        rows.push_back((k + 1 < phases.size() ? phases[k + 1] : phases[0] + beat) - phases[k]);
    return Groove{shortestUnit(rows), (int) phases.size(), phases.front(), agreement};
}

/** The beat of a tune no row grid holds through: one with a row a frame longer now and then, a
    groove that varies, or parts at other speeds. A grid needs every row to start where the first
    rows put it; how alike the tune is to itself a while later needs no such thing, and peaks at
    the pulse (a row, or a groove's round) and at every beat.

    The pulse is the shortest lag that repeats nearly as well as the best one. The beat is that many
    frames times a power of two, inside the BPM range, or three pulses times a power of two when
    that repeats clearly better over 1, 2 and 4 beats. The pulse stands for the rows: what lies
    inside it is not known. When no such beat repeats, the pulse may not be a whole number of
    frames (see driftingBeat): the beat found by following the peaks then stands for the pulse.
    Nothing when even that beat repeats poorly. With the beat known, the rows inside it are read
    where stretches of the tune agree on them (see readGroove), and stand in place of the pulse. */
PulseSearch
detectPulse(const std::vector<double>& energy, double total, const std::vector<double>& repeat, double dataFps) {
    const int low = minBeatFrames, high = maxBeatFrames;
    PulseSearch search;
    if (repeat.empty())
        return search;
    const double best = *std::max_element(repeat.begin() + 2, repeat.begin() + 2 * high + 1);
    int pulse = 0;
    for (int lag = 2; lag <= 2 * high && pulse == 0; ++lag) {
        const auto at = (size_t) lag;
        if (best > 0.0 && repeat[at] >= pulseStrength * best && repeat[at] >= repeat[at - 1]
            && repeat[at] >= repeat[at + 1])
            pulse = lag;
    }
    while (pulse > high && pulse % 2 == 0)
        pulse /= 2;
    if (pulse == 0 || pulse > high)
        return search;
    search.pulse = pulse;

    auto over = [&](int beat) { return *repeatOverBeats(repeat, beat); };  // a beat within `high` is in reach
    auto strongest = [&](int unit) {  // of the unit's doublings inside the BPM range; 0 when none is
        int chosen = 0;
        for (int beat = unit; beat <= high; beat *= 2)
            if (beat >= low && (chosen == 0 || over(beat) >= over(chosen)))
                chosen = beat;
        return chosen;
    };
    int beat = strongest(pulse);
    const int triple = strongest(3 * pulse);
    for (int candidate : {beat, triple})
        if (candidate != 0)
            search.tried.push_back({candidate / pulse, candidate, {}, {}, over(candidate)});
    if (triple != 0 && (beat == 0 || over(triple) > over(beat) + beatMargin))
        beat = triple;
    int unit = pulse;  // what stands for the rows
    if (beat == 0 || over(beat) < minBeatRepeat) {
        const auto [drifting, repeats] = driftingBeat(repeat, pulse);
        if (drifting != 0)
            search.tried.push_back({juce::roundToInt((double) drifting / pulse), drifting, {}, {}, repeats});
        if (drifting == 0 || repeats < minBeatRepeat)
            return search;
        beat = unit = drifting;
    }

    if (const auto groove = readGroove(energy, beat)) {
        Speed speed{groove->rows, groove->phase, fitGrid(energy, total, beat, groove->rowsPerBeat).share,
                    groove->rowsPerBeat};
        while (dataFps * 60.0 / beat * speed.beatRows / groove->rowsPerBeat > tracktion::TempoSetting::maxBPM)
            speed.beatRows *= 2;
        search.grooveAgreement = groove->agreement;
        search.chosen = speed;
        return search;
    }

    const auto fit = fitGrid(energy, total, unit, 1);
    Speed speed{{unit}, fit.phases[0], fit.share, beat / unit};
    while (dataFps * 60.0 / (unit * speed.beatRows) > tracktion::TempoSetting::maxBPM)
        speed.beatRows *= 2;
    search.chosen = speed;
    return search;
}

constexpr std::array<size_t, 4> grooveSizes{1, 2, 4, 8};  // rows a module's row pattern may have
constexpr double minRowsFollowing = 0.9;                  // share of a module's rows its pattern must describe
constexpr double minPatternsFilled = 0.9;                 // share of a module in patterns a beat or a bar fills
constexpr double minPatternsOnPhase = 2.0 / 3.0;          // share of a module in patterns that start its bars

/** One round of a pulse: the tune cut, from its first row on, into runs of whole rows that
    last `pulse` frames each; a row that starts no such run is passed over. */
struct PulseRows
{
    std::vector<int> rows;  // the commonest run, cut to its shortest repeating unit; empty when nothing lasts a pulse
    double tiled = 0.0;     // share of the rows inside such runs
};

PulseRows rowsOfPulse(const std::vector<int>& lengths, int pulse) {
    std::map<std::vector<int>, size_t> runs;
    size_t covered = 0;
    for (size_t i = 0; i < lengths.size();) {
        int frames = 0;
        size_t k = 0;
        while (i + k < lengths.size() && k < grooveSizes.back() && frames < pulse)
            frames += lengths[i + k++];
        if (frames != pulse) {
            ++i;
            continue;
        }
        ++runs[std::vector<int>(lengths.begin() + (std::ptrdiff_t) i, lengths.begin() + (std::ptrdiff_t) (i + k))];
        covered += k;
        i += k;
    }
    if (runs.empty())
        return {};
    return {shortestUnit(commonestOf(runs)->first), (double) covered / (double) lengths.size()};
}

/** The pattern turned to the row it starts on for most of the tune's rows, counted from the first. */
std::vector<int> turnedToRows(std::vector<int> rows, const std::vector<int>& lengths) {
    size_t bestTurn = 0, bestFollow = 0;
    for (size_t turn = 0; turn < rows.size(); ++turn) {
        size_t follow = 0;
        for (size_t i = 0; i < lengths.size(); ++i)
            follow += lengths[i] == rows[(i + turn) % rows.size()] ? 1 : 0;
        if (follow > bestFollow) {
            bestFollow = follow;
            bestTurn = turn;
        }
    }
    std::rotate(rows.begin(), rows.begin() + (std::ptrdiff_t) bestTurn, rows.end());
    return rows;
}

/** The rows of a module's beat, from how many frames `rowsPerBeat` rows usually last: of the runs
    of that many rows that last the commonest number of frames, the one that occurs most, cut to
    its shortest repeating unit and turned to the rows it starts on. A module whose rows follow no
    short pattern still has a beat of so many frames: rows of 4-4-4-4-4-4-4-3 make one of 31, not
    of eight times the commonest row. Empty when the tune has no more rows than a beat, or when
    one of that run's rows is more than a frame off `commonestRow`: a pause, a change of speed or
    another part of the tune, not the beat that many rows were taken to make. */
std::vector<int> rowsOfBeat(const std::vector<size_t>& starts, int rowsPerBeat, int commonestRow) {
    const auto n = (size_t) std::max(0, rowsPerBeat);
    if (n == 0 || starts.size() <= n)
        return {};
    const auto lengths = rowLengths(starts);
    std::map<size_t, size_t> spans;
    for (size_t i = 0; i + n < starts.size(); ++i)
        ++spans[starts[i + n] - starts[i]];
    const auto span = commonestOf(spans)->first;
    std::map<std::vector<int>, size_t> runs;
    for (size_t i = 0; i + n < starts.size(); ++i)
        if (starts[i + n] - starts[i] == span)
            ++runs[std::vector<int>(lengths.begin() + (std::ptrdiff_t) i, lengths.begin() + (std::ptrdiff_t) (i + n))];
    const auto run = commonestOf(runs)->first;
    if (std::any_of(run.begin(), run.end(), [&](int row) { return std::abs(row - commonestRow) > 1; }))
        return {};
    return turnedToRows(shortestUnit(run), lengths);
}

/** A module's patterns, which start bars: how many rows each holds and how many frames it lasts,
    the last one left out, since where it ends is not known. */
struct Patterns
{
    std::vector<size_t> rows, frames;
    std::vector<size_t> starts;  // the frame each of them starts at

    Patterns(const std::vector<size_t>& rowStarts, const std::vector<size_t>& patternStarts) {
        auto rowAt = [&](size_t frame) {
            const auto row = std::lower_bound(rowStarts.begin(), rowStarts.end(), frame);
            return (size_t) std::distance(rowStarts.begin(), row);
        };
        for (size_t k = 1; k < patternStarts.size(); ++k) {
            rows.push_back(rowAt(patternStarts[k]) - rowAt(patternStarts[k - 1]));
            frames.push_back(patternStarts[k] - patternStarts[k - 1]);
            starts.push_back(patternStarts[k - 1]);
        }
    }

    /** The frames before the first bar line, for bars of `bar` frames: where in such a bar the
        patterns that hold two thirds of the tune start. A pickup, or an intro of an odd length,
        puts that off the tune's first row. Nothing when no one place holds that much: a beat that
        is not a whole number of frames, patterns of odd lengths all through, or too few patterns
        to tell. */
    juce::Optional<int> barPhase(int bar) const {
        if (!known() || bar <= 0)
            return {};
        std::map<size_t, size_t> held;  // place in the bar -> frames in the patterns that start there
        size_t all = 0;
        for (size_t k = 0; k < starts.size(); ++k) {
            held[starts[k] % (size_t) bar] += frames[k];
            all += frames[k];
        }
        const auto most = commonestOf(held);
        if ((double) most->second < minPatternsOnPhase * (double) all)
            return {};
        return (int) most->first;
    }

    bool known() const { return rows.size() > 1; }

    /** Whether a unit, a beat or a bar, of so many rows lasting so many frames fills the patterns:
        nearly all the tune lies in patterns that hold a whole number of it, counted in rows or in
        frames. Either will do: the rows of a pattern may differ in length, and the beat is then a
        time; a beat may not be a whole number of frames (64 rows lasting 252), and it is then so
        many rows. A unit that fills them neither way is not the tune's. True when too few patterns
        are known to tell. */
    bool filledBy(int unitRows, int unitFrames) const {
        return !known() || wholeIn(rows, unitRows) || wholeIn(frames, unitFrames);
    }

private:
    static bool wholeIn(const std::vector<size_t>& lengths, int unit) {
        if (unit <= 0)
            return false;
        size_t whole = 0, all = 0;
        for (auto length : lengths) {
            all += length;
            whole += length % (size_t) unit == 0 ? length : 0;
        }
        return (double) whole >= minPatternsFilled * (double) all;
    }
};

/** A tracker module's own row grid, from the frame every row starts at: the shortest
    of 1, 2, 4 or 8 row lengths, repeating from the first row, that 90% of the rows follow
    (an intro or an ending at another speed aside). A module knows its rows, so a swing as
    wide as speeds 2 and 6, or a groove of 8 rows (3-3-3-2-3-2-3-2), is a grid too.

    Rows that don't repeat by count still keep a pulse: a groove lasts the same frames every
    time round, whether an intro at speed 4 comes before 5-5-3-3 or a 3-5-3-3 turns into 4-4-3-3
    now and then. So next comes the shortest pulse the rows repeat after (90% of the row starts
    have another one that many frames later), then the shortest one the tune is made of (runs of
    whole rows lasting a pulse hold 90% of the rows), each with the rows that fill it most often.
    Else the tune has no one grid, and the commonest row length stands for it. */
Speed speedOfRows(const std::vector<size_t>& starts) {
    const auto lengths = rowLengths(starts);
    auto commonest = [&](size_t n, size_t k) {
        std::map<int, size_t> counts;
        for (size_t i = k; i < lengths.size(); i += n)
            ++counts[lengths[i]];
        return commonestOf(counts)->first;
    };
    const auto enough = minRowsFollowing * (double) lengths.size();
    for (size_t n : grooveSizes) {
        if (lengths.size() < n)
            break;
        std::vector<int> rows;
        for (size_t k = 0; k < n; ++k)
            rows.push_back(commonest(n, k));
        size_t follow = 0;
        for (size_t i = 0; i < lengths.size(); ++i)
            follow += lengths[i] == rows[i % n] ? 1 : 0;
        if ((double) follow >= enough)
            return Speed{rows, (int) starts.front(), 1.0};
    }

    // frames -> the row starts that have another row start that many frames on, within a groove's rows
    std::map<int, size_t> reach;
    for (size_t i = 0; i < lengths.size(); ++i) {
        int frames = 0;
        for (size_t k = 0; k < grooveSizes.back() && i + k < lengths.size(); ++k)
            ++reach[frames += lengths[i + k]];
    }
    for (bool repeating : {true, false})
        for (const auto& [pulse, count] : reach) {
            if (pulse <= 0 || (repeating && (double) count < enough))
                continue;
            const auto found = rowsOfPulse(lengths, pulse);
            const bool isGroove = std::find(grooveSizes.begin(), grooveSizes.end(), found.rows.size()) != grooveSizes.end();
            if (isGroove && (repeating || found.tiled >= minRowsFollowing))
                return Speed{turnedToRows(found.rows, lengths), (int) starts.front(), 1.0};
        }
    Speed guess{{commonest(1, 0)}, (int) starts.front(), 1.0};
    guess.guessed = true;
    return guess;
}

/** Frame index of every row start. */
std::vector<size_t> rowStarts(size_t numFrames, const Speed& speed) {
    std::vector<size_t> starts;
    for (size_t i = (size_t) speed.phase, k = 0; i < numFrames; i += (size_t) speed.rows[k++ % speed.rows.size()])
        starts.push_back(i);
    return starts;
}

/** One chip's drums at a row start: channels playing noise, the noise period, an
    envelope retrigger within the row. */
struct Drums
{
    std::array<bool, 3> noiseChannels{};
    int noisePeriod = -1;
    bool retrigger = false;

    bool operator==(const Drums&) const = default;
};

/** What a row sounds like at its start, coarse enough to repeat bar after bar:
    the drums of every chip and the bass (the lowest note the Notes view would draw
    on any chip, an envelope pitch included, in semitones of its period). */
struct RowFeature
{
    std::vector<Drums> drums;  // one per chip
    int bassSemitone = -1;

    bool drumsEqual(const RowFeature& o) const { return drums == o.drums; }
};

std::vector<RowFeature> rowFeatures(const Chips& chips, const std::vector<size_t>& starts) {
    std::vector<RowFeature> rows(starts.size(), RowFeature{std::vector<Drums>(chips.size()), -1});
    for (size_t chip = 0; chip < chips.size(); ++chip) {
        const auto& data = *chips[chip];
        uZX::PsgRegsFrame state;
        // Same two-stage accumulation as PsgList::loadFrom, so the bass is the note the Notes
        // view would draw lowest (envelope pitches included): registers first, so a frame
        // that writes only a fine period byte keeps its coarse byte.
        uZX::PsgRegsFrame regsState;
        PsgParamFrameData params;
        params.resetMixer();
        size_t row = 0;

        for (size_t i = 0; i < data.frames.size() && row < starts.size(); ++i) {
            state.update(data.frames[i]);
            regsState.clear();
            regsState.update(data.frames[i]);
            params.clear();
            params.update(regsState);
            if (row + 1 < starts.size() && i >= starts[row + 1])
                ++row;
            auto& drums = rows[row].drums[chip];
            drums.retrigger = drums.retrigger || data.frames[i].isSet(uZX::PsgRegType::EnvelopeShape);
            if (i != starts[row])
                continue;

            const auto mixer = state.getMixer();
            for (size_t ch = 0; ch < 3; ++ch) {
                const bool audible = (state.getRaw(uZX::PsgRegType::VolumeA + ch) & 0x1f) != 0;
                drums.noiseChannels[ch] = audible && ((mixer >> (3 + ch)) & 1) == 0;
            }
            if (drums.noiseChannels[0] || drums.noiseChannels[1] || drums.noiseChannels[2])
                drums.noisePeriod = state.getNoisePeriod();
            int bassPeriod = 0;
            visitFrameNotes(params, [&](const PsgFrameNote& note) { bassPeriod = std::max(bassPeriod, note.period); });
            if (bassPeriod > 0)
                rows[row].bassSemitone =
                    std::max(rows[row].bassSemitone, juce::roundToInt(12.0 * std::log2((double) bassPeriod)));
        }
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
    its accents (note starts per row) to recur at its own length. A beat of 3, 4, 5
    or 7 rows also wins when the row evidence is clearly more alike itself that far
    on (`repeat`, its autocorrelation) than at the beat chosen so. A swing pair or
    groove never splits across beats, so those tunes always group by 4, or by a
    module's 8-row groove. A module's patterns start bars, so a beat fills them
    (see Patterns::filledBy): of the beats tried, those that would leave patterns
    a part of a beat are out, unless that is every one of them. */
BeatSearch detectRowsPerBeat(const std::vector<RowFeature>& rows,
                             const std::vector<double>& accents,
                             const Speed& speed,
                             double dataFps,
                             const std::vector<double>& repeat,
                             const Patterns& patterns) {
    const bool swing = speed.rows.size() > 1;
    const double rowLength = speed.meanRow();
    auto fitToBpmRange = [&](int rowsPerBeat) {
        const int minRowsPerBeat = (int) speed.rows.size();
        double bpm = bpmRangeFps * 60.0 / (rowLength * rowsPerBeat);
        while (bpm < minBpm && rowsPerBeat % 2 == 0 && rowsPerBeat / 2 >= minRowsPerBeat) {
            rowsPerBeat /= 2;
            bpm *= 2.0;
        }
        while (bpm > maxBpm) {
            rowsPerBeat *= 2;
            bpm /= 2.0;
        }
        while (dataFps * 60.0 / (rowLength * rowsPerBeat) > tracktion::TempoSetting::maxBPM)
            rowsPerBeat *= 2;
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

    auto alike = [&](int rowsPerBeat) { return repeatOverBeats(repeat, juce::roundToInt(rowLength * rowsPerBeat)); };
    const double accentScale = accentRecurrence(0);
    auto weighed = [&](int rowsPerBeat) {
        return Analysis::Beat{rowsPerBeat,
                              juce::roundToInt(rowLength * rowsPerBeat),
                              repeats(rowsPerBeat),
                              accentScale > 0.0 ? accentRecurrence(rowsPerBeat) / accentScale : 0.0,
                              alike(rowsPerBeat)};
    };

    const int fourRowBeat = fitToBpmRange(std::max(4, (int) speed.rows.size()));
    if (swing)
        return {fourRowBeat, {weighed(fourRowBeat)}};
    BeatSearch search{fourRowBeat, {weighed(fourRowBeat)}};
    for (int group : {3, 5, 7})
        search.tried.push_back(weighed(fitToBpmRange(group)));
    auto fillsThem = [&](int rowsPerBeat) {
        return patterns.filledBy(rowsPerBeat, juce::roundToInt(rowLength * rowsPerBeat));
    };
    const bool someFill = std::any_of(search.tried.begin(), search.tried.end(), [&](const Analysis::Beat& beat) {
        return fillsThem(beat.rowsPerBeat);
    });
    auto allowed = [&](int rowsPerBeat) { return !someFill || fillsThem(rowsPerBeat); };

    int best = fourRowBeat;
    bool fills = allowed(best);  // false: any beat that fills the patterns replaces it
    double bestScore = repeats(fourRowBeat);
    for (int group : {3, 5}) {
        const int beat = fitToBpmRange(group);
        if (!allowed(beat))
            continue;
        const double score = repeats(beat);
        const double accent = accentRecurrence(beat);
        if (!fills || (score > bestScore && accent > 0.0 && accent > accentRecurrence(fourRowBeat))) {
            best = beat;
            bestScore = score;
            fills = true;
        }
    }

    // Drums and bass repeat much alike for beats built of the same rows, and a beat of 7 rows is
    // none of the three. The row evidence tells them apart: the tune is most alike itself one, two
    // and four beats on. A beat too long for `repeat` is left to the above.
    if (auto score = alike(best))
        for (int group : {4, 3, 5, 7}) {
            const int beat = fitToBpmRange(group);
            if (const auto other = alike(beat); allowed(beat) && other && (!fills || *other > *score + beatMargin)) {
                best = beat;
                score = other;
                fills = true;
            }
        }
    if (!fills)  // only a beat too long for `repeat` fills the patterns
        for (const auto& beat : search.tried)
            if (allowed(beat.rowsPerBeat)) {
                best = beat.rowsPerBeat;
                break;
            }
    search.rowsPerBeat = best;
    return search;
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

namespace {

/** The chips play one tune: what each frame holds adds up over them. */
std::vector<double> summedOverChips(const Chips& chips, std::vector<double> (*perFrame)(const uZX::PsgData&)) {
    std::vector<double> sum;
    for (const auto* chip : chips) {
        const auto values = perFrame(*chip);
        sum.resize(std::max(sum.size(), values.size()), 0.0);
        for (size_t i = 0; i < values.size(); ++i)
            sum[i] += values[i];
    }
    return sum;
}

/** The row grid of a per-frame energy; nothing is tried when the tune holds less of it than `least`. */
GridSearch gridOf(const std::vector<double>& energy, double least) {
    const double total = std::accumulate(energy.begin(), energy.end(), 0.0);
    if (total < least)
        return {};
    return detectSpeed(energy, total);
}

}  // namespace

juce::Optional<double> repeatOverBeats(const std::vector<double>& repeat, int beatFrames) {
    const auto beat = (size_t) std::max(0, beatFrames);
    if (beat == 0 || 4 * beat >= repeat.size())
        return {};
    return (repeat[beat] + repeat[2 * beat] + repeat[4 * beat]) / 3.0;
}

double gridShare(std::span<const double> energy, int period) {
    const double total = std::accumulate(energy.begin(), energy.end(), 0.0);
    return total > 0.0 && period > 0 ? fitGrid(energy, total, period, 1).share : 0.0;
}

juce::Optional<Result> detect(const uZX::PsgData& data) {
    return detect(Chips{&data});
}

juce::Optional<Result> detect(const std::vector<const uZX::PsgData*>& chips) {
    return detect(chips, {});
}

juce::Optional<Result> detect(const std::vector<const uZX::PsgData*>& chips,
                              const std::vector<size_t>& knownRows,
                              const std::vector<size_t>& knownPatterns) {
    return analyse(chips, knownRows, knownPatterns).result;
}

Analysis analyse(const std::vector<const uZX::PsgData*>& chips,
                 const std::vector<size_t>& knownRows,
                 const std::vector<size_t>& knownPatterns) {
    using RowsFrom = Analysis::RowsFrom;
    Analysis analysis;
    if (chips.empty())
        return analysis;
    const bool rowsKnown = knownRows.size() > 1;
    const Patterns patterns(knownRows, rowsKnown ? knownPatterns : std::vector<size_t>{});
    analysis.noteStarts = summedOverChips(chips, onsetEnergy);
    const auto& noteStarts = analysis.noteStarts;
    const auto& data = *chips.front();  // the chips of one tune share the frame rate
    const auto playRate = (int) std::max<size_t>(1, data.options.playRate);  // machine frames per data frame
    const double dataFps = data.getFrameRate() / playRate;
    // The rows are the module's own, else the grid of the learnt row evidence, else, when that
    // shows none, the grid of the note starts: cruder, but they don't lean on how a tracker's
    // player moves the registers. A tune no grid holds through still has a pulse.
    analysis.evidence = summedOverChips(chips, PsgRowEvidence::of);
    const auto& evidence = analysis.evidence;
    // how alike the row evidence is to itself up to four beats on; empty for a tune too short to tell
    if (evidence.size() > (size_t) (8 * maxBeatFrames))
        analysis.repeat = autocorrelation(evidence, (size_t) (4 * maxBeatFrames));
    const auto& repeat = analysis.repeat;
    const auto* energy = &noteStarts;  // what the rows were found in
    juce::Optional<Speed> speed;
    if (rowsKnown) {
        speed = speedOfRows(knownRows);
        analysis.rowsFrom = RowsFrom::module;
    } else {
        const double total = std::accumulate(evidence.begin(), evidence.end(), 0.0);
        const auto grids = gridOf(evidence, minEvidence);
        for (const auto& grid : grids.tried)
            analysis.grids.push_back({grid.rows, grid.share});
        speed = grids.chosen;
        if (speed) {
            energy = &evidence;
            analysis.rowsFrom = RowsFrom::evidence;
        } else if ((speed = gridOf(noteStarts, minEnergy).chosen)) {
            analysis.rowsFrom = RowsFrom::noteStarts;
        } else if (total >= minEvidence) {
            auto pulse = detectPulse(evidence, total, repeat, dataFps);
            speed = pulse.chosen;
            energy = &evidence;
            analysis.pulse = pulse.pulse;
            analysis.grooveAgreement = pulse.grooveAgreement;
            analysis.beats = std::move(pulse.tried);
            if (speed)
                analysis.rowsFrom = RowsFrom::pulse;
        }
    }
    if (!speed)
        return analysis;

    Result result;
    for (int row : speed->rows)
        result.rowFrames.push_back(row * playRate);
    const auto starts = rowsKnown ? knownRows : rowStarts(energy->size(), *speed);
    const auto rows = rowFeatures(chips, starts);
    std::vector<double> accents;  // how loud a row's notes start, whatever found the rows
    for (auto i : starts)
        accents.push_back(noteStarts[i]);
    result.rowsArePulse = speed->beatRows > 0;
    if (result.rowsArePulse) {
        result.rowsPerBeat = speed->beatRows;
    } else {
        auto beats = detectRowsPerBeat(rows, accents, *speed, dataFps, repeat, patterns);
        result.rowsPerBeat = beats.rowsPerBeat;
        analysis.beats = std::move(beats.tried);
    }
    // A module whose rows follow no pattern: its beat lasts as long as that many of them usually do
    if (const auto beat = speed->guessed ? rowsOfBeat(knownRows, result.rowsPerBeat, speed->rows.front())
                                         : std::vector<int>{};
        !beat.empty()) {
        result.rowFrames.clear();
        for (int row : beat)
            result.rowFrames.push_back(row * playRate);
    }
    const auto meterMargin = tripleMeterMargin(rows, result.rowsPerBeat);
    analysis.meterMargin = meterMargin;
    result.beatsPerBar = meterMargin > minMeterMargin ? 3 : 4;
    result.meterClear = std::abs(meterMargin) > minMeterMargin;
    for (int row = 0; row < result.rowsPerBeat; ++row)
        result.framesPerBeat += result.rowFrames[(size_t) row % result.rowFrames.size()];
    result.bpm = data.getFrameRate() * 60.0 / result.framesPerBeat;
    // A module's patterns start bars: when bars of only 3 or only 4 beats fill them, that is the meter
    if (patterns.known()) {
        const int rowsPerBeat = result.rowsPerBeat, beat = result.framesPerBeat / playRate;
        const bool three = patterns.filledBy(3 * rowsPerBeat, 3 * beat);
        if (three != patterns.filledBy(4 * rowsPerBeat, 4 * beat)) {
            result.beatsPerBar = three ? 3 : 4;
            result.meterClear = true;
        }
    }
    result.gridShare = speed->share;
    const auto barFrames = (size_t) (result.framesPerBeat * result.beatsPerBar / playRate);
    const int firstRow = speed->phase;
    result.gridPhase = firstRow * playRate;
    // A grid that holds through the tune places the clip only when the opening follows it: an intro at
    // another speed would give a bogus lead-in. A tune with only a pulse hardly ever has its opening on
    // that for certain, so a clear pulse places the clip by itself: it holds the evidence, and the beat
    // has several, so the first pulse is the downbeat as the first row is. The one pulse of a beat says
    // only where the tune is loudest, which may be the backbeat, and places nothing.
    const bool pulsePlaces = result.rowsArePulse && result.rowsPerBeat > 1 && speed->share >= minPulseShare;
    if (rowsKnown || pulsePlaces || openingFollowsGrid(*energy, *speed, 2 * barFrames))
        result.firstRowFrame = firstRow * playRate;
    // A module's patterns start bars: the first bar line is where most of them start, not the first row
    // when a pickup or an intro of an odd length comes before
    int leadIn = firstRow;
    if (const auto phase = patterns.barPhase((int) barFrames); phase && *phase != firstRow % (int) barFrames) {
        leadIn = *phase;
        result.firstRowFrame = leadIn * playRate;
    }

    for (const auto* chip : chips) {
        uZX::PsgRegsFrame state;
        for (size_t i = 0; i < (size_t) leadIn && i < chip->frames.size(); ++i) {
            state.update(chip->frames[i]);
            for (size_t ch = 0; ch < 3; ++ch)
                if ((state.getRaw(uZX::PsgRegType::VolumeA + ch) & 0x1f) != 0)
                    result.silentLeadIn = false;
        }
    }
    analysis.result = result;
    return analysis;
}

namespace {

/** Whether the clips are alone in their edit: tempo and meter are only taken from
    the files when nothing else depends on them. */
bool areOnlyClipsInEdit(const juce::Array<PsgClip*>& clips) {
    auto isOther = [&](tracktion::Clip* other) {
        return other != nullptr && ! clips.contains(dynamic_cast<PsgClip*>(other));
    };
    for (auto* track : tracktion::getClipTracks(clips.getFirst()->edit)) {
        for (auto* other : track->getClips())
            if (isOther(other))
                return false;
        if (auto* audioTrack = dynamic_cast<tracktion::AudioTrack*>(track))
            for (auto* slot : audioTrack->getClipSlotList().getClipSlots())
                if (isOther(slot->getClip()))
                    return false;
    }
    return true;
}

}  // namespace

void applyTempo(const juce::Array<PsgClip*>& clips, const Result& result) {
    auto& edit = clips.getFirst()->edit;
    const auto clipStart = clips.getFirst()->getPosition().getStart();
    PsgTiming::setFramesPerBeat(edit,
                                edit.tempoSequence.getTempoAt(clipStart),
                                result.framesPerBeat,
                                /*preserveAbsoluteFrameTimes=*/true);
    // a beat that is one pulse has no groove of its own: it gets the grid's usual groups
    const auto groove = result.subdivisionPattern();
    Helpers::setGridSubdivisionPattern(
        edit, groove.size() > 1 ? groove : Helpers::getDefaultGridSubdivisionPattern(result.framesPerBeat));
    if (result.meterClear)
        edit.tempoSequence.getTimeSigAt(clipStart).numerator = result.beatsPerBar;
}

void applyToEdit(const juce::Array<PsgClip*>& clips,
                 const std::vector<const uZX::PsgData*>& chips,
                 const std::vector<size_t>& knownRows,
                 const std::vector<size_t>& knownPatterns) {
    if (clips.isEmpty() || !areOnlyClipsInEdit(clips))
        return;
    const auto result = detect(chips, knownRows, knownPatterns);
    if (!result)
        return;

    applyTempo(clips, *result);
    auto& edit = clips.getFirst()->edit;
    auto& timeSig = edit.tempoSequence.getTimeSigAt(clips.getFirst()->getPosition().getStart());

    if (result->firstRowFrame <= 0)
        return;

    // Every clip moves alike, so the chips of a Turbo Sound tune stay in step.
    const auto fps = Helpers::getProjectFps(edit);
    for (auto* clip : clips) {
        auto position = clip->getPosition();
        if (result->silentLeadIn) {
            const auto lead = tracktion::TimeDuration::fromSeconds(result->firstRowFrame / fps);
            position.time = {position.getStart(), position.getEnd() - lead};
            position.offset = position.offset + lead;
            clip->setPosition(position);
        } else {
            // The clip stays on its bar line and grows by the rests that come before the lead-in
            const int barFrames = result->framesPerBeat * timeSig.numerator.get();
            const int shift = barFrames - result->firstRowFrame % barFrames;
            const auto rests = tracktion::TimeDuration::fromSeconds(shift / fps);
            clip->setLeadInFrames(juce::roundToInt(rests.inSeconds() * clip->getPsg().getEffectiveFps(*clip)),
                                  &edit.getUndoManager());
            clip->setLength(position.getLength() + rests, false);
        }
    }
}

void setFrameRate(const juce::Array<PsgClip*>& clips, double fps) {
    if (clips.isEmpty() || fps <= 0.0)
        return;
    auto* first = clips.getFirst();
    const auto oldFps = first->getPsg().getEffectiveFps(*first);
    for (auto* clip : clips)
        PsgTiming::setClipFrameRate(*clip, fps);
    if (oldFps <= 0.0 || ! areOnlyClipsInEdit(clips))
        return;

    // Alone in its project, the tune sets it: the whole timeline scales with the rate. The clips
    // start as many frames in, the tempo scales, so a beat keeps its frames and the grid its groove,
    // and a first row on a bar line stays on it. Past Tracktion's tempo limits a beat takes twice or
    // half the frames.
    for (auto* clip : clips)
        clip->setStart(tracktion::TimePosition::fromSeconds(clip->getPosition().getStart().inSeconds() * oldFps / fps),
                       false, true);
    auto& edit = first->edit;
    auto& tempo = edit.tempoSequence.getTempoAt(first->getPosition().getStart());
    auto pattern = Helpers::getGridSubdivisionPattern(edit, juce::jmax(1, juce::roundToInt(PsgTiming::getFramesPerBeat(edit))));
    auto bpm = tempo.getBpm() * fps / oldFps;
    bool regroup = false;
    for (; bpm > tracktion::TempoSetting::maxBPM; bpm /= 2.0, regroup = true) {
        const auto beat = pattern;
        pattern.insert(pattern.end(), beat.begin(), beat.end());
    }
    for (; bpm < tracktion::TempoSetting::minBPM; bpm *= 2.0, regroup = true)
        pattern.clear();  // ponytail: a beat shorter than its groove goes back to the default grid
    Helpers::setProjectFps(edit, fps);
    PsgTiming::setTempoBpmRetimingFrames(edit, tempo, bpm, /*preserveAbsoluteFrameTimes=*/true);
    if (regroup)
        Helpers::setGridSubdivisionPattern(edit, pattern.empty()
            ? Helpers::getDefaultGridSubdivisionPattern(juce::jmax(1, juce::roundToInt(PsgTiming::getFramesPerBeat(edit))))
            : pattern);
}

void setProjectFrameRate(tracktion::Edit& edit, double fps) {
    const auto oldFps = Helpers::getProjectFps(edit);
    juce::Array<PsgClip*> clips;
    for (auto* track : tracktion::getClipTracks(edit))
        for (auto* clip : track->getClips())
            if (auto* psg = dynamic_cast<PsgClip*>(clip); psg != nullptr && ! psg->getPsg().hasFpsMismatch(*psg, oldFps))
                clips.add(psg);
    Helpers::setProjectFps(edit, fps);
    setFrameRate(clips, fps);
}

}  // namespace MoTool::PsgTempo
