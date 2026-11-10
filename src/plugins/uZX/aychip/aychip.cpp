
#include <JuceHeader.h>

#include "aychip.h"

#include <cmath>
#include <vector>

namespace MoTool::uZX {


void AYInterface::setRegister(size_t index, unsigned char value) noexcept {
    if (index >= numRegisters) {
        return;
    }
    regs_[index] = value;
    switch (index) {
        case 0: case 1: setTonePeriod(0, periodAt(0)); break;
        case 2: case 3: setTonePeriod(1, periodAt(2)); break;
        case 4: case 5: setTonePeriod(2, periodAt(4)); break;
        case 6: setNoisePeriod(value); break;
        case 7:
            for (int chan = 0; chan < 3; ++chan) {
                setChannelFromRegisters(chan);
            }
            break;
        case 8: case 9: case 10: setChannelFromRegisters(static_cast<int>(index) - 8); break;
        case 11: case 12: setEnvelopePeriod(periodAt(11)); break;
        case 13: setEnvelopeShape(static_cast<EnvShape>(value)); break;
    }
}

unsigned char AYInterface::getRegister(size_t index) const noexcept {
    return index < numRegisters ? regs_[index] : 0;
}

void AYInterface::setChannelFromRegisters(int chan) noexcept {
    //   7   |   6   |    5    |    4    |    3    |   2    |   1    |   0
    // I/O B | I/O A | Noise C | Noise B | Noise A | Tone C | Tone B | Tone A
    const auto mixer = regs_[7];
    const auto volume = regs_[8 + static_cast<size_t>(chan)];
    setToneOn(chan, ! ((mixer >> chan) & 1));
    setNoiseOn(chan, ! ((mixer >> (chan + 3)) & 1));
    setVolume(chan, volume & 0x0f);
    setEnvelopeOn(chan, volume & 0x10);
}

void AYInterface::applyChannelMute(int chan, bool toneEnabled, bool noiseEnabled, bool envelopeEnabled, bool channelEnabled) noexcept {
    if (chan < 0 || chan >= 3) {
        return;
    }

    // Mutes apply over the registers as written, so a channel comes back when they turn off
    setChannelFromRegisters(chan);
    if (!channelEnabled) {
        // When channel is disabled, set volume to 0 and envelope off
        // to prevent audible clicks (tone-off sets signal high)
        setVolume(chan, 0);
        setEnvelopeOn(chan, false);
    } else {
        // Channel is enabled, but check individual effects
        if (!toneEnabled) {
            setToneOn(chan, false);
        }
        if (!noiseEnabled) {
            setNoiseOn(chan, false);
        }
        if (!envelopeEnabled) {
            setEnvelopeOn(chan, false);
        }
    }
}

AyumiEmulator::AyumiEmulator(int sampleRate, double clock, ChipType type)
    : AYInterface()
    , Pan_ {0.25, 0.75, 0.5}  // ACB is default
    , MasterVolume_(1.0)
{
    reset(sampleRate, clock, type);
}

AyumiEmulator::~AyumiEmulator() {

}

// Mute all notes, used in pause mode
auto AyumiEmulator::muteSound() -> void {
    // set volume and envelope off for all channels
    // no not set mixer to all bits off, because it also disables tone and noise generators
    // we want only to set volume to 0 for tone and env to off
    setRegister(8, 0);
    setRegister(9, 0);
    setRegister(10, 0);
}

auto AyumiEmulator::reset(int sampleRate, double clock, ChipType type) -> void {
    SampleRate_ = sampleRate;
    ClockRate_ = clock;
    Type_ = type;
    auto result = ayumi_configure(&Ayumi_, type, clock, sampleRate);
    if (result != 1) {
        std::cerr << "ayumi_configure with sample rate " << sampleRate
                  << " and clock rate " << clock
                  << " failed with code " << result
                  << std::endl;
        jassertfalse;  // configuration failed
    } else {
        // std::cerr << "ayumi_configure with sample rate " << sampleRate
        //           << " and clock rate " << clock
        //           << " succeeded" << std::endl;
    }
    ignoreUnused(result);
    for (int i = 0; i < TONE_CHANNELS; ++i) {
        setChannelPan(i, Pan_[i]);
    }
    for (size_t i = 0; i < numRegisters; ++i) {
        setRegister(i, i == 7 ? 0x3f : 0);  // tone and noise off, silent
    }
}

auto AyumiEmulator::reconfigure(int sampleRate, double clock, ChipType type) -> void {
    std::array<unsigned char, numRegisters> regs;
    for (size_t i = 0; i < numRegisters; ++i) {
        regs[i] = getRegister(i);
    }
    reset(sampleRate, clock, type);
    for (size_t i = 0; i < numRegisters; ++i) {
        setRegister(i, regs[i]);
    }
}

auto AyumiEmulator::canChangeClock() const -> bool {
    return true;
}

auto AyumiEmulator::canChangeClockContinously() const -> bool {
    return true;
}

auto AyumiEmulator::getClockValues() const -> std::vector<float> {
    return {};  // no values because canChangeClockContinously() == true
}

auto AyumiEmulator::setSampleRate(int sampleRate) -> void {
    reconfigure(sampleRate, ClockRate_, Type_);
}

auto AyumiEmulator::getSampleRate() const -> int {
    return SampleRate_;
}

auto AyumiEmulator::setType(ChipType type) -> void {
    reconfigure(SampleRate_, ClockRate_, type);
}

auto AyumiEmulator::getType() const -> ChipType {
    return Type_;
}

auto AyumiEmulator::getClock() const -> double {
    return ClockRate_;
}

auto AyumiEmulator::setClock(double rate) -> void {
    reconfigure(SampleRate_, rate, Type_);
}

auto AyumiEmulator::setLayoutAndStereoWidth(ChannelsLayout layout, double stereoWidth) -> void {
    std::array<double, TONE_CHANNELS> pan = {0.0, 0.0, 0.0};
    pan = ChannelPans_[static_cast<size_t>(layout)];
    for (size_t i = 0; i < TONE_CHANNELS; ++i) {
        // use stereoWidth to adjust pan.
        // stereoWidth == 0.0 — pan is 0.5,
        // stereoWidth == 1.0 — pan is pan
        // stereoWidth == 0.5 — pan is halfway to 0.5
        setChannelPan(static_cast<int>(i), 0.5 + (pan[i] - 0.5) * stereoWidth);
    }
    ChannelsLayout_ = layout;
    StereoWidth_ = stereoWidth;
}

auto AyumiEmulator::getLayout() -> ChannelsLayout {
    return ChannelsLayout_;
}

auto AyumiEmulator::getStereoWidth() -> double {
    return StereoWidth_;
}

auto AyumiEmulator::setChannelPan(int chan, double pan, bool isEqp) -> void {
    // 1.0 is right, 0.0 is left
    Pan_[chan] = pan;
    ayumi_set_pan(&Ayumi_, chan, pan, isEqp);
}

auto AyumiEmulator::getChannelPan(int chan) const -> double {
    return Pan_[chan];
}

auto AyumiEmulator::setTonePeriod(int chan, int period) -> void {
    ayumi_set_tone(&Ayumi_, chan, period);
}

auto AyumiEmulator::getTonePeriod(int chan) const -> int {
    return Ayumi_.channels[chan].tone_period;
}

auto AyumiEmulator::getEnvelopePeriod() const -> int {
    return Ayumi_.envelope_period;
}

auto AyumiEmulator::setNoisePeriod(int period) -> void {
    ayumi_set_noise(&Ayumi_, period);
}

auto AyumiEmulator::getNoisePeriod() const -> int {
    return Ayumi_.noise_period;
}

auto AyumiEmulator::setEnvelopePeriod(int period) -> void {
    return ayumi_set_envelope(&Ayumi_, period);
}

auto AyumiEmulator::setEnvelopeShape(EnvShape shape) -> void {
    ayumi_set_envelope_shape(&Ayumi_, shape);
}

auto AyumiEmulator::getEnvelopeShape() const -> EnvShape {
    return Ayumi_.envelope_shape;
}

auto AyumiEmulator::setEnvelopeOn(int chan, bool on) -> void {
    Ayumi_.channels[chan].e_on = on;
}

auto AyumiEmulator::setNoiseOn(int chan, bool on) -> void {
    Ayumi_.channels[chan].n_off = !on;
}

auto AyumiEmulator::setMixer(int chan, bool tOn, bool nOn, bool eOn) -> void {
    ayumi_set_mixer(&Ayumi_, chan, !tOn, !nOn, eOn);
}

auto AyumiEmulator::getMixer(int chan) const -> std::tuple<bool, bool, bool> {
    return { !Ayumi_.channels[chan].t_off, !Ayumi_.channels[chan].n_off, Ayumi_.channels[chan].e_on };
}

auto AyumiEmulator::setVolume(int chan, int volume) -> void {
    ayumi_set_volume(&Ayumi_, chan, volume);
}

auto AyumiEmulator::getVolume(int chan) const -> int {
    return Ayumi_.channels[chan].volume;
}

auto AyumiEmulator::setToneOn(int chan, bool on) -> void {
    Ayumi_.channels[chan].t_off = !on;
}

auto AyumiEmulator::setMasterVolume(float volume) -> void {
    MasterVolume_ = volume;
}

auto AyumiEmulator::getMasterVolume() const -> float {
    return MasterVolume_;
}

auto AyumiEmulator::processBlockStereo(float* outLeft, float* outRight, size_t numSamples, bool removeDC,
                                       const ScopeLanes& scope) -> void {
    float* const out[] {outLeft, outRight};
    render(AYUMI_STEREO, out, numSamples, removeDC, scope);
}

auto AyumiEmulator::processBlockSeparate(float* outA, float* outB, float* outC, size_t numSamples, bool removeDC,
                                         const ScopeLanes& scope) -> void {
    float* const out[] {outA, outB, outC};
    render(AYUMI_SEPARATE, out, numSamples, removeDC, scope);
}

void AyumiEmulator::render(ayumi_output_mode mode, float* const* out, size_t numSamples, bool removeDC,
                           const ScopeLanes& scope) {
    // A switch clicks once: the filters it brings in still hold whatever they rendered last
    ayumi_set_output_mode(&Ayumi_, mode);
    const auto numOutputs = static_cast<size_t>(mode);
    for (size_t i = 0; i < numSamples; ++i) {
        ayumi_process(&Ayumi_);
        if (removeDC) {
            ayumi_remove_dc(&Ayumi_);
        }
        for (size_t ch = 0; ch < numOutputs; ++ch) {
            out[ch][i] = static_cast<float>(Ayumi_.outputs[ch].value) * MasterVolume_;
        }
        for (size_t ch = 0; ch < TONE_CHANNELS; ++ch) {
            const auto& c = Ayumi_.channels[ch];
            if (scope.levels != nullptr) {
                scope.levels[ch][i] = static_cast<float>(Ayumi_.raw[ch]);
            }
            if (scope.sync != nullptr) {
                const int bit = ! c.t_off ? c.tone : c.e_on ? Ayumi_.envelope_segment : 0;
                scope.sync[ch][i] = static_cast<float>(bit);
            }
            if (scope.amplitude != nullptr) {
                scope.amplitude[ch][i] = c.e_on ? 1.0f : static_cast<float>(Ayumi_.dac_table[c.volume * 2 + 1]);
            }
        }
    }
}

} // namespace MoTool::uZX
