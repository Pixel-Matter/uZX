#include "PsgSoundingStates.h"

#include "ClipTimers.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace MoTool {

float getPsgVolumeLevel(int volume, bool envelope) noexcept {
    if (envelope)
        return 1.0f;
    return volume > 0 ? std::exp2(static_cast<float>(volume - 15) / 3.0f) : 0.0f;
}

float getTimerLevel(const TimerSpan& span) noexcept {
    float level = -1.0f;
    for (size_t chan = 0; chan < span.loudestVolumes.size(); ++chan)
        if ((span.registers & (1u << (8 + chan))) != 0)
            level = jmax(level, getPsgVolumeLevel(span.loudestVolumes[chan] & 0x0F, (span.loudestVolumes[chan] & 0x10) != 0));
    return level >= 0.0f ? level : 1.0f;
}

namespace {

using T = PsgParamType;

int8_t getEnvelopeChannel(const PsgParamFrameData& data) noexcept {
    int owner = -1;
    for (int ch = 0; ch < 3; ++ch) {
        if (data.getRaw(T(T::EnvelopeIsOnA + ch)) == 0)
            continue;
        if (owner >= 0)
            return -1;
        owner = ch;
    }
    if (owner < 0)
        return -1;
    const bool toneApart = data.getRaw(T(T::ToneIsOnA + owner)) > 0
        && data.getRaw(T(T::TonePeriodA + owner)) != envelopePeriodToTonePeriod(data.getRaw(T::EnvelopePeriod));
    return toneApart ? int8_t {-1} : static_cast<int8_t>(owner);
}

}  // namespace

void PsgSoundingStates::add(int from, int to, int storedFrame, bool startsFrame, const PsgParamFrameData& data,
                            const ClipTimers& timers) {
    PsgSoundingState state;
    state.from = from;
    state.to = to;
    state.storedFrame = storedFrame;
    state.envelopeShape = static_cast<uint8_t>(data.getRaw(T::EnvelopeShape));
    state.envelopeRestarts = startsFrame && data.isSet(T::RetriggerEnvelope) && data.getRaw(T::RetriggerEnvelope) != 0;
    state.envelopeChannel = getEnvelopeChannel(data);
    state.noisePeriod = static_cast<uint8_t>(data.getRaw(T::NoisePeriod));

    const bool envelopeIsLow = envelopePeriodToTonePeriod(data.getRaw(T::EnvelopePeriod)) > T {T::TonePeriodA}.getScale().end;
    for (int ch = 0; ch < 3; ++ch) {
        const bool onEnvelope = data.getRaw(T(T::EnvelopeIsOnA + ch)) > 0;
        if (envelopeIsLow && onEnvelope)
            state.lowEnvelopeChannels |= static_cast<uint8_t>(1u << ch);
        if (data.getRaw(T(T::NoiseIsOnA + ch)) > 0)
            state.noiseLevels[static_cast<size_t>(ch)] = getPsgVolumeLevel(data.getRaw(T(T::VolumeA + ch)), onEnvelope);
    }
    state.noiseLevel = *std::max_element(state.noiseLevels.begin(), state.noiseLevels.end());

    const auto firstNote = notes_.size();
    state.firstNote = static_cast<uint32_t>(firstNote);
    state.firstBar = static_cast<uint32_t>(bars_.size());

    visitFrameNotes(data, [&] (const PsgFrameNote& n) {
        state.pitchChannels |= n.channelMask;
        if (! n.isEnvelopePitch)
            state.toneChannels |= n.channelMask;
        notes_.push_back({n.pitch, getPsgVolumeLevel(n.volume, n.hasEnvMod), n.channelMask, n.isEnvelopePitch,
                          n.hasEnvMod, nullptr});
    });

    // A timer whose writes are a pitch is a note among the frame's; else a bar
    for (int timer = 0; timer < timers.getNumTimers(); ++timer) {
        const auto* span = timers.findSpan(timer, from);
        if (span == nullptr)
            continue;
        if (const auto pitch = span->getPitch()) {
            state.timerChannels |= span->channels;
            state.pitchChannels |= span->channels;
            notes_.push_back({*pitch, getTimerLevel(*span), span->channels, false, false, span});
        } else {
            bars_.push_back(span);
        }
    }

    const auto order = [] (const PsgSoundingNote& n) {
        return std::tuple(-n.pitch, n.isEnvelopePitch, n.timer != nullptr ? n.timer->timer : -1, n.channelMask);
    };
    std::sort(notes_.begin() + static_cast<std::ptrdiff_t>(firstNote), notes_.end(),
              [&] (const PsgSoundingNote& a, const PsgSoundingNote& b) { return order(a) < order(b); });

    state.numNotes = static_cast<uint8_t>(notes_.size() - firstNote);
    state.numBars = static_cast<uint8_t>(bars_.size() - state.firstBar);
    states_.push_back(state);
}

}  // namespace MoTool
