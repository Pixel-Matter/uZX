#include "PsgRowEvidence.h"

#include <algorithm>
#include <cmath>

namespace MoTool::PsgRowEvidence {

namespace {

// How many values each part of a cell takes; a cell is its parts in this order, the first the most significant.
constexpr int pitchClasses = 9, mixerClasses = 9, envelopeClasses = 3;
constexpr int envelopePeriodClasses = 3, noisePeriodClasses = 2;

// In a PT3 every envelope restart starts a row, which is how its player works, not what a
// restart means: other players restart the envelope inside rows. So a restart alone raises
// a frame to this chance of starting a row at most, whatever the corpus says.
constexpr double maxRestartProbability = 0.8;

// The same goes for a note's end. A PT3 cuts a note with a command on a row; elsewhere a note
// lasts as many frames as its sample or a tick count says. A channel that falls silent never
// speaks for a row start.
constexpr int volumeFellSilent = 7;  // see volumeClass

// rowStartShare, channelWeights and chipWeights, written by scripts/train_row_evidence.py
#include "PsgRowEvidenceTable.inc"

double logit(double p) {
    return std::log(p / (1.0 - p));
}

/** How a channel's volume moved: 0 same, 1 up by 1-3, 2 up by 4 or more, 3 and 4 the same from
    silence, 5 down by 1-3, 6 down by 4 or more, 7 down to silence. */
int volumeClass(int volume, int prevVolume) {
    const int step = volume - prevVolume;
    if (step > 0)
        return (step >= 4 ? 2 : 1) + (prevVolume == 0 ? 2 : 0);
    if (step < 0)
        return volume == 0 ? 7 : step <= -4 ? 6 : 5;
    return 0;
}

/** How a sounding tone's pitch moved, in semitones: 1 same, 2 under a half, 3 under one (a
    slide), then a jump after the pitch had held 1, 2, 3-4, or 5 and more frames (4 to 7): an
    ornament jumps every frame or two, a new note after a held one. 8 a tone that appears, 0
    no tone on either frame. */
int pitchClass(bool sounding, bool prevSounding, double semitones, int heldFrames) {
    if (sounding && !prevSounding)
        return 8;
    if (!sounding || !prevSounding)
        return 0;
    if (semitones < 0.05)
        return 1;
    if (semitones < 0.5)
        return 2;
    if (semitones < 1.0 - 1e-9)
        return 3;
    return heldFrames <= 1 ? 4 : heldFrames == 2 ? 5 : heldFrames <= 4 ? 6 : 7;
}

/** 1 switched on, 2 switched off, 0 neither; only while the channel is audible. */
int switched(bool audible, bool on, bool prevOn) {
    return !audible || on == prevOn ? 0 : on ? 1 : 2;
}

}  // namespace

std::vector<FrameCells> cellsOf(const uZX::PsgData& data) {
    std::vector<FrameCells> cells(data.frames.size());
    uZX::PsgRegsFrame state, prev;
    std::array<bool, 3> prevSounding{};
    std::array<double, 3> prevPitch{};
    std::array<int, 3> stableFrames{};  // frames in a row, up to the last one, that kept the channel's pitch
    bool prevRestart = false;
    double prevEnvelopePitch = 0.0;

    for (size_t i = 0; i < data.frames.size(); ++i) {
        prev = state;
        state.update(data.frames[i]);
        const int mixer = state.getMixer(), prevMixer = prev.getMixer();
        bool anyEnvelope = false, anyNoise = false;

        for (size_t ch = 0; ch < 3; ++ch) {
            const int raw = state.getRaw(uZX::PsgRegType::VolumeA + ch);
            const int prevRaw = prev.getRaw(uZX::PsgRegType::VolumeA + ch);
            const int volume = raw & 0x0f, prevVolume = prevRaw & 0x0f;
            const bool envelope = (raw & 0x10) != 0, prevEnvelope = (prevRaw & 0x10) != 0;
            const bool toneOn = ((mixer >> ch) & 1) == 0, prevToneOn = ((prevMixer >> ch) & 1) == 0;
            const bool noiseOn = ((mixer >> (3 + ch)) & 1) == 0, prevNoiseOn = ((prevMixer >> (3 + ch)) & 1) == 0;
            const int period = state.getTonePeriod(ch) & 0x0fff;
            const bool audible = volume > 0 || envelope;
            const bool sounding = audible && toneOn && period > 0;
            const double pitch = sounding ? 12.0 * std::log2((double) period) : 0.0;
            const double moved = std::abs(pitch - prevPitch[ch]);

            const int parts[] = {volumeClass(volume, prevVolume),
                                 pitchClass(sounding, prevSounding[ch], moved, stableFrames[ch] + 1),
                                 switched(audible, noiseOn, prevNoiseOn) * 3 + switched(audible, toneOn, prevToneOn),
                                 envelope == prevEnvelope ? 0
                                 : envelope               ? 1
                                                          : 2};
            cells[i].channels[ch] =
                (uint16_t) (((parts[0] * pitchClasses + parts[1]) * mixerClasses + parts[2]) * envelopeClasses
                            + parts[3]);

            stableFrames[ch] = sounding && prevSounding[ch] && moved < 0.5 ? stableFrames[ch] + 1 : 0;
            prevSounding[ch] = sounding;
            prevPitch[ch] = pitch;
            anyEnvelope = anyEnvelope || envelope;
            anyNoise = anyNoise || (audible && noiseOn);
        }

        // 1 a restart, 2 one that follows another the frame before, 3 one no channel uses
        const bool restart = data.frames[i].isSet(uZX::PsgRegType::EnvelopeShape);
        const int restartClass = !restart ? 0 : !anyEnvelope ? 3 : prevRestart ? 2 : 1;
        const double envelopePitch = 12.0 * std::log2((double) std::max<int>(1, state.getEnvelopePeriod()));
        const bool envelopeMoved = anyEnvelope && state.getEnvelopePeriod() != prev.getEnvelopePeriod();
        const int envelopePeriodClass = !envelopeMoved ? 0 : std::abs(envelopePitch - prevEnvelopePitch) >= 0.5 ? 2 : 1;
        const bool noiseMoved = anyNoise && (state.getNoisePeriod() & 0x1f) != (prev.getNoisePeriod() & 0x1f);
        cells[i].chip = (uint16_t) ((restartClass * envelopePeriodClasses + envelopePeriodClass) * noisePeriodClasses
                                    + (noiseMoved ? 1 : 0));
        prevRestart = restart;
        prevEnvelopePitch = envelopePitch;
    }
    if (!cells.empty())
        cells.front() = {};
    return cells;
}

std::vector<double> of(const uZX::PsgData& data) {
    // Weights are logs of odds against the share of frames that start a row, so a frame is
    // likelier than not to start one from this sum up. Below it the frame counts for nothing:
    // faint evidence that recurs at one place in every row would otherwise pass for rows.
    static const double likelierThanNot = -logit(rowStartShare);
    static const double restartCap = logit(maxRestartProbability) - logit(rowStartShare);
    const auto cells = cellsOf(data);
    std::vector<double> evidence(cells.size());
    for (size_t i = 0; i < cells.size(); ++i) {
        const bool restart = cells[i].chip / (envelopePeriodClasses * noisePeriodClasses) > 0;
        double weight =
            restart ? std::min((double) chipWeights[cells[i].chip], restartCap) : chipWeights[cells[i].chip];
        for (auto cell : cells[i].channels) {
            const bool fellSilent = cell / (pitchClasses * mixerClasses * envelopeClasses) == volumeFellSilent;
            weight += fellSilent ? std::min(channelWeights[cell], 0.0f) : channelWeights[cell];
        }
        evidence[i] = weight >= likelierThanNot ? weight : 0.0;
    }
    return evidence;
}

}  // namespace MoTool::PsgRowEvidence
