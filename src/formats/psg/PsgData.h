#pragma once

#include "juce_core/juce_core.h"
#include <JuceHeader.h>

#include <cstdint>
#include <array>
#include <vector>

namespace MoTool::uZX {

template <typename T, size_t NREGS>
struct PsgDeltaBase {
    std::array<T, NREGS> registers {};
    std::array<bool, NREGS> mask {};

    PsgDeltaBase() = default;
    PsgDeltaBase(const PsgDeltaBase&) = default;
    PsgDeltaBase(PsgDeltaBase&&) = default;
    PsgDeltaBase& operator=(const PsgDeltaBase&) = default;
    PsgDeltaBase& operator=(PsgDeltaBase&&) = default;
    PsgDeltaBase(std::array<T, NREGS>&& regs, std::array<bool, NREGS>&& m)
        : registers(std::move(regs)), mask(std::move(m)) {}

    inline constexpr static size_t size() noexcept {
        return NREGS;
    }

    inline constexpr bool isSet(size_t index) const noexcept {
        return mask[index];
    }

    inline constexpr T getRaw(size_t index) const noexcept {
        return registers[index];
    }

    inline void constexpr clear() noexcept {
        mask.fill(false);
    }

};

struct PsgRegsFrame : public PsgDeltaBase<uint8_t, 14> {
    enum PsgRegisterType {
        TonePeriodFineA      = 0,
        TonePeriodCoarseA    = 1,
        TonePeriodFineB      = 2,
        TonePeriodCoarseB    = 3,
        TonePeriodFineC      = 4,
        TonePeriodCoarseC    = 5,
        NoisePeriod          = 6,
        Mixer                = 7,
        VolumeA              = 8,
        VolumeB              = 9,
        VolumeC              = 10,
        EnvelopePeriodFine   = 11,
        EnvelopePeriodCoarse = 12,
        EnvelopeShape        = 13
    };

    using PsgDeltaBase::PsgDeltaBase;

    inline bool empty() const {
        return mask == std::array<bool, 14> {false};
    }
    // Helpers for common register access
    inline bool hasTonePeriodSet(size_t chan) const {
        return mask[TonePeriodFineA + chan * 2] || mask[TonePeriodCoarseA + chan * 2];
    }
    inline uint8_t getTonePeriodFine(size_t chan) const {
        return registers[TonePeriodFineA + chan * 2];
    }
    inline uint8_t getTonePeriodCoarse(size_t chan) const {
        return registers[TonePeriodCoarseA + chan * 2];
    }
    inline void setTonePeriodFine(size_t chan, uint8_t period) {
        registers[TonePeriodFineA + chan * 2] = period;
        mask[TonePeriodFineA + chan * 2] = true;
    }
    inline void setTonePeriodCoarse(size_t chan, uint8_t period) {
        registers[TonePeriodCoarseA + chan * 2] = period;
        mask[TonePeriodCoarseA + chan * 2] = true;
    }
    inline void setTonePeriod(size_t chan, uint16_t period) {
        setTonePeriodCoarse(chan, period >> 8);
        setTonePeriodFine(chan, period & 0xff);
    }
    inline uint16_t getTonePeriod(size_t chan) const {
        return static_cast<uint16_t>(getTonePeriodCoarse(chan) << 8) | getTonePeriodFine(chan);
    }
    inline bool hasNoisePeriodSet() const {
        return mask[NoisePeriod];
    }
    inline uint8_t getNoisePeriod() const {
        return registers[NoisePeriod];
    }
    inline void setNoisePeriod(uint8_t period) {
        registers[NoisePeriod] = period;
        mask[NoisePeriod] = true;
    }
    inline bool hasMixerSet() const {
        return mask[Mixer];
    }
    inline uint8_t getMixer() const {
        return registers[Mixer];
    }
    inline void setMixer(uint8_t mixer) {
        registers[Mixer] = mixer;
        mask[Mixer] = true;
    }
    inline bool getToneOn(size_t chan) const {
        return !(registers[Mixer] & (1 << chan));  // inverted
    }
    inline void setToneOn(size_t chan, bool on) {
        if (!on) {  // inverted
            registers[Mixer] |= (1 << chan);
        } else {
            registers[Mixer] &= ~(1 << chan);
        }
        mask[Mixer] = true;
    }
    inline bool getNoiseOn(size_t chan) const {
        return !(registers[Mixer] & (0x08 << chan));  // inverted
    }
    inline void setNoiseOn(size_t chan, bool on) {
        if (!on) {  // inverted
            registers[Mixer] |= (0x08 << chan);
        } else {
            registers[Mixer] &= ~(0x08 << chan);
        }
        mask[Mixer] = true;
    }
    inline bool hasVolumeOrEnvModSet(size_t chan) const {
        return mask[VolumeA + chan];
    }
    inline bool hasVolumeSet(size_t chan) const {
        return mask[VolumeA + chan] && (registers[VolumeA + chan] & 0xF);
    }
    inline uint8_t getVolumeAndEnvMod(size_t chan) const {
        return registers[VolumeA + chan];
    }
    inline bool getEnvMod(size_t chan) const {
        return registers[VolumeA + chan] & 0x10;
    }
    inline uint8_t getVolume(size_t chan) const {
        return registers[VolumeA + chan] & 0x0F;
    }
    inline void setVolumeAndEnvMod(size_t chan, uint8_t volume, bool envMod) {
        registers[VolumeA + chan] = (volume & 0x0F) | (envMod ? 0x10 : 0);
        mask[VolumeA + chan] = true;
    }
    inline void setEnvMod(size_t chan, bool envMod) {
        registers[VolumeA + chan] = (registers[VolumeA + chan] & 0x0F) | (envMod ? 0x10 : 0);
        mask[VolumeA + chan] = true;
    }
    inline void setVolume(size_t chan, uint8_t volume) {
        registers[VolumeA + chan] |= volume & 0x0F;
        mask[VolumeA + chan] = true;
    }
    inline bool hasEnvelopePeriodSet() const {
        return mask[EnvelopePeriodFine] || mask[EnvelopePeriodCoarse];
    }
    inline uint8_t getEnvelopePeriodFine() const {
        return registers[EnvelopePeriodFine];
    }
    inline uint8_t getEnvelopePeriodCoarse() const {
        return registers[EnvelopePeriodCoarse];
    }
    inline void setEnvelopePeriodFine(uint8_t period) {
        registers[EnvelopePeriodFine] = period;
        mask[EnvelopePeriodFine] = true;
    }
    inline void setEnvelopePeriodCoarse(uint8_t period) {
        registers[EnvelopePeriodCoarse] = period;
        mask[EnvelopePeriodCoarse] = true;
    }
    inline void setEnvelopePeriod(uint16_t period) {
        setEnvelopePeriodCoarse(period >> 8);
        setEnvelopePeriodFine(period & 0xff);
    }
    inline uint16_t getEnvelopePeriod() const {
        return static_cast<uint16_t>(getEnvelopePeriodCoarse() << 8) | getEnvelopePeriodFine();
    }
    inline bool hasEnvelopeShapeSet() const {
        return mask[EnvelopeShape];
    }
    inline uint8_t getEnvelopeShape() const {
        return registers[EnvelopeShape];
    }
    inline void setEnvelopeShape(uint8_t shape) {
        registers[EnvelopeShape] = shape;
        mask[EnvelopeShape] = true;
    }

    void debugPrint() const {
        for (size_t i = 0; i < registers.size(); ++i) {
            DBG("PsgRegsFrame: " << i << ": " << static_cast<int>(registers[i]) << " " << (mask[i] ? "true" : "false"));
        }
    }
};

struct PsgData {
    struct Options {
        double frameRate = 50;
        size_t frameStep = 1;
        // bool usesRetriggers = false;
        // TODO frequency table or id or shared_ptr to it
    };

    std::vector<PsgRegsFrame> frames;
    Options options;

    void clear() {
        frames.clear();
    }

    size_t getLengthMachineFrames() const noexcept {
        return frames.size() * options.frameStep;
    }

    double getLengthSeconds() const noexcept {
        return static_cast<double>(getLengthMachineFrames()) / options.frameRate;
    }

    inline bool isEmpty() const noexcept {
        return frames.empty();
    }

    inline double getFrameRate() const {
        return options.frameRate;
    }

    inline double frameNumToSeconds(size_t frameNum) const {
        return static_cast<double>(frameNum) / getFrameRate();
    }
};


} // namespace MoTool::uZX