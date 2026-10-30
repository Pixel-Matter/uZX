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

struct Speed
{
    int a, b, phase;
    double share;
};

/** Every divisor of the true speed also holds, so the largest period wins, but a
    multiple only beats its divisors if it keeps (almost) all of their energy;
    otherwise the tune just has most of its notes on even rows. */
juce::Optional<Speed> detectSpeed(const std::vector<double>& energy, double total) {
    std::array<double, maxSpeed + 1> share{};
    juce::Optional<Speed> best;

    for (int p = minSpeed; p <= maxSpeed; ++p) {
        const auto fit = gridShare(energy, total, p, 1);
        share[(size_t) p] = fit.share;
        double divisors = 0.0;
        for (int d = minSpeed; d < p; ++d)
            if (p % d == 0)
                divisors = std::max(divisors, share[(size_t) d]);
        if (fit.share >= minGridShare && fit.share >= divisors - harmonicTolerance)
            best = Speed{p, p, fit.phases[0], fit.share};
    }
    if (best)
        return best;

    // two-value swing: period a+b with two phases a apart, a and b near each other
    for (int period = 2 * minSpeed + 1; period <= 2 * maxSpeed; ++period) {
        const auto fit = gridShare(energy, total, period, 2);
        const int a = fit.phases[1] - fit.phases[0];
        const int b = period - a;
        const bool plausible = a >= minSpeed && a <= maxSpeed && b >= minSpeed && b <= maxSpeed && std::abs(a - b) <= 2;
        if (plausible && fit.share >= minGridShare && (!best || fit.share > best->share))
            best = Speed{a, b, fit.phases[0], fit.share};
    }
    return best;
}

/** Picks 3 or 4 rows per beat from the row-accent autocorrelation (duple unless
    triplets clearly win), then halves or doubles to land inside the BPM range. */
int detectRowsPerBeat(const std::vector<double>& energy, const Speed& speed, double dataFps) {
    std::vector<double> rows;
    for (size_t i = (size_t) speed.phase, k = 0; i < energy.size(); i += (size_t) (k++ % 2 == 0 ? speed.a : speed.b))
        rows.push_back(energy[i]);

    const double mean = rows.empty() ? 0.0 : std::accumulate(rows.begin(), rows.end(), 0.0) / (double) rows.size();
    for (auto& r : rows)
        r -= mean;
    auto autocorrelation = [&](size_t lag) {
        double sum = 0.0;
        for (size_t i = lag; i < rows.size(); ++i)
            sum += rows[i - lag] * rows[i];
        return sum;
    };

    const double triplet = autocorrelation(3), duple = autocorrelation(4);
    int rowsPerBeat = triplet > 0.0 && triplet > duple * 1.2 ? 3 : 4;
    const double rowLength = (speed.a + speed.b) / 2.0;
    double bpm = dataFps * 60.0 / (rowLength * rowsPerBeat);
    while (bpm < minBpm && rowsPerBeat > 1) {
        rowsPerBeat /= 2;
        bpm *= 2.0;
    }
    while (bpm > maxBpm) {
        rowsPerBeat *= 2;
        bpm /= 2.0;
    }
    return rowsPerBeat;
}

}  // namespace

std::vector<int> Result::subdivisionPattern() const {
    std::vector<int> pattern;
    for (int row = 0; row < rowsPerBeat; ++row)
        pattern.push_back(row % 2 == 0 ? speedA : speedB);
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
    result.speedA = speed->a * playRate;
    result.speedB = speed->b * playRate;
    result.rowsPerBeat = detectRowsPerBeat(energy, *speed, data.getFrameRate() / playRate);
    const auto pattern = result.subdivisionPattern();
    result.framesPerBeat = std::accumulate(pattern.begin(), pattern.end(), 0);
    result.bpm = data.getFrameRate() * 60.0 / result.framesPerBeat;
    result.gridShare = speed->share;
    return result;
}

void applyToEdit(PsgClip& clip, const Result& result) {
    auto& edit = clip.edit;
    for (auto* track : te::getClipTracks(edit))
        for (auto* other : track->getClips())
            if (other != &clip && dynamic_cast<PsgClip*>(other) != nullptr)
                return;

    PsgTiming::setFramesPerBeat(edit, result.framesPerBeat, /*preserveAbsoluteFrameTimes=*/true);
    Helpers::setGridSubdivisionPattern(edit, result.subdivisionPattern());
}

}  // namespace MoTool::PsgTempo
