
#include <JuceHeader.h>

#include "aychip.h"

#include <cmath>
#include <vector>

namespace MoTool::uZX {


void AYInterface::setRegister(size_t index, unsigned char value) noexcept {
    if (index >= 14) {
        return;
    }
    switch (index) {
        case 0: setR0(value); break;
        case 1: setR1(value); break;
        case 2: setR2(value); break;
        case 3: setR3(value); break;
        case 4: setR4(value); break;
        case 5: setR5(value); break;
        case 6: setR6(value); break;
        case 7: setR7(value); break;
        case 8: setR8(value); break;
        case 9: setR9(value); break;
        case 10: setR10(value); break;
        case 11: setR11(value); break;
        case 12: setR12(value); break;
        case 13: setR13(value); break;
    }
}

void AYInterface::applyChannelMute(int chan, bool toneEnabled, bool noiseEnabled, bool envelopeEnabled, bool channelEnabled) noexcept {
    if (chan < 0 || chan >= 3) {
        return;
    }

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

AyumiEmulator::AyumiEmulator(int sampleRate, double clock, ChipType type, int numChannels)
    : AYInterface()
    , Pan_ {0.25, 0.75, 0.5}  // ACB is default
    , MasterVolume_(1.0)
{
    reset(sampleRate, clock, type, numChannels);
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

auto AyumiEmulator::reset(int sampleRate, double clock, ChipType type, int numChannels) -> void {
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
    setOutputMode(numChannels);
    for (int i = 0; i < TONE_CHANNELS; ++i) {
        setChannelPan(i, Pan_[i]);
        setMixer(i, false, false, false);
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
    reset(sampleRate, ClockRate_, Type_);
}

auto AyumiEmulator::getSampleRate() const -> int {
    return SampleRate_;
}

auto AyumiEmulator::setType(ChipType type) -> void {
    reset(SampleRate_, ClockRate_, type);
}

auto AyumiEmulator::getType() const -> ChipType {
    return Type_;
}

auto AyumiEmulator::getClock() const -> double {
    return ClockRate_;
}

auto AyumiEmulator::setClock(double rate) -> void {
    reset(SampleRate_, rate, Type_);
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

auto AyumiEmulator::setOutputMode(int numChannels) -> void {
    enum ayumi_output_mode mode;
    switch (numChannels) {
        case 1: mode = AYUMI_MONO; break;
        case 2: mode = AYUMI_STEREO; break;
        case 3: mode = AYUMI_SEPARATE; break;
        default:
            jassertfalse;  // Invalid number of channels
            return;
    }
    ayumi_set_output_mode(&Ayumi_, mode);
}

auto AyumiEmulator::processBlockMono(float* outMono, size_t numSamples, bool removeDC, size_t stride) -> void {
    for (size_t i = 0; i < numSamples; ++i, outMono+=stride) {
        ayumi_process(&Ayumi_);
        if (removeDC) {
            ayumi_remove_dc(&Ayumi_);
        }
        double mono, unused;
        ayumi_get_stereo_output(&Ayumi_, &mono, &unused);
        *outMono = static_cast<float>(mono) * MasterVolume_;
    }
}

auto AyumiEmulator::processBlockStereo(float* outLeft, float* outRight, size_t numSamples, bool removeDC, size_t stride) -> void {
    for (size_t i = 0; i < numSamples; ++i, outLeft+=stride, outRight+=stride) {
        ayumi_process(&Ayumi_);
        if (removeDC) {
            ayumi_remove_dc(&Ayumi_);
        }
        double left, right;
        ayumi_get_stereo_output(&Ayumi_, &left, &right);
        *outLeft = static_cast<float>(left) * MasterVolume_;
        *outRight = static_cast<float>(right) * MasterVolume_;
    }
}

auto AyumiEmulator::processBlockSeparate(float* outCh0, float* outCh1, float* outCh2, size_t numSamples, bool removeDC, size_t stride) -> void {
    jassert(Ayumi_.output_mode == AYUMI_SEPARATE);  // Must call setOutputMode(3) first
    for (size_t i = 0; i < numSamples; ++i, outCh0+=stride, outCh1+=stride, outCh2+=stride) {
        ayumi_process(&Ayumi_);
        if (removeDC) {
            ayumi_remove_dc(&Ayumi_);
        }
        double ch0, ch1, ch2;
        ayumi_get_separate_output(&Ayumi_, &ch0, &ch1, &ch2);
        *outCh0 = static_cast<float>(ch0) * MasterVolume_;
        *outCh1 = static_cast<float>(ch1) * MasterVolume_;
        *outCh2 = static_cast<float>(ch2) * MasterVolume_;
    }
}

auto AyumiEmulator::processBlockStereoPlusSeparate(float* outLeft, float* outRight, float* outCh0, float* outCh1, float* outCh2,
                                                   size_t numSamples, bool removeDC, size_t stride) -> void {
    if (Ayumi_.output_mode != AYUMI_SEPARATE) {
        DBG("WARNING: processBlockStereoPlusSeparate called but output_mode=" << Ayumi_.output_mode << " (expected AYUMI_SEPARATE=2)");
    }

    float maxLeft = 0.0f, maxRight = 0.0f;

    for (size_t i = 0; i < numSamples; ++i, outLeft+=stride, outRight+=stride, outCh0+=stride, outCh1+=stride, outCh2+=stride) {
        ayumi_process(&Ayumi_);
        if (removeDC) {
            ayumi_remove_dc(&Ayumi_);
        }

        // Get separate channel outputs
        double ch0, ch1, ch2;
        ayumi_get_separate_output(&Ayumi_, &ch0, &ch1, &ch2);
        *outCh0 = static_cast<float>(ch0) * MasterVolume_;
        *outCh1 = static_cast<float>(ch1) * MasterVolume_;
        *outCh2 = static_cast<float>(ch2) * MasterVolume_;

        // Get stereo output (ayumi_get_stereo_output handles mixing in SEPARATE mode)
        double left, right;
        ayumi_get_stereo_output(&Ayumi_, &left, &right);
        *outLeft = static_cast<float>(left) * MasterVolume_;
        *outRight = static_cast<float>(right) * MasterVolume_;
    }
}

} // namespace MoTool::uZX
