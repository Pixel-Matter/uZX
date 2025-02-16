// #include <string.h>
// #include <math.h>

#include "aychip.h"

#include <cmath>
#include <vector>

namespace uZX::Chip {


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

AyumiEmulator::AyumiEmulator(int sampleRate, double clock, ChipType type)
    : AYInterface()
    , Pan_ {0.25, 0.75, 0.5}  // ACB is default
    , MasterVolume_(1.0)
{
    Reset(sampleRate, clock, type);
}

AyumiEmulator::~AyumiEmulator() {

}

auto AyumiEmulator::Reset(int sampleRate, double clock, ChipType type) -> void {
    SampleRate_ = sampleRate;
    ClockRate_ = clock;
    Type_ = type;
    ayumi_configure(&Ayumi_, type, clock, sampleRate);
    for (int i = 0; i < TONE_CHANNELS; ++i) {
        setPan(i, Pan_[i]);
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
    Reset(sampleRate, ClockRate_, Type_);
}

auto AyumiEmulator::getSampleRate() const -> int {
    return SampleRate_;
}

auto AyumiEmulator::setType(ChipType type) -> void {
    Reset(SampleRate_, ClockRate_, type);
}

auto AyumiEmulator::getType() const -> ChipType {
    return Type_;
}

auto AyumiEmulator::getClock() const -> double {
    return ClockRate_;
}

auto AyumiEmulator::setClock(double rate) -> void {
    Reset(SampleRate_, rate, Type_);
}

auto AyumiEmulator::setPan(int chan, double pan, bool isEqp) -> void {
    // 1.0 is right, 0.0 is left
    Pan_[chan] = pan;
    ayumi_set_pan(&Ayumi_, chan, pan, isEqp);
}

auto AyumiEmulator::getPan(int chan) const -> double {
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

auto AyumiEmulator::processBlock(float* outLeft, float* outRight, size_t numSamples, bool removeDC, size_t stride) -> void {
    for (size_t i = 0; i < numSamples; ++i, outLeft+=stride, outRight+=stride) {
        ayumi_process(&Ayumi_);
        if (removeDC) {
            ayumi_remove_dc(&Ayumi_);
        }
        *outLeft = static_cast<float>(Ayumi_.left) * MasterVolume_;
        *outRight = static_cast<float>(Ayumi_.right) * MasterVolume_;
    }
}

}
