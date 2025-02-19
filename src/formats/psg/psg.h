#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <array>
#include <vector>

namespace te = tracktion;

namespace MoTool::uZX {

template <size_t NREGS>
struct PSGFrame {
    std::array<uint8_t, NREGS> registers;
    std::array<bool, NREGS> mask;
};


struct PSGRegsAYFrame : public PSGFrame<14> {
    // Helpers for common register access
    inline bool hasTonePeriodSet(size_t chan) const {
        return mask[0 + chan * 2] || mask[1 + chan * 2];
    }
    inline uint8_t getTonePeriodFine(size_t chan) const {
        return registers[0 + chan * 2];
    }
    inline uint8_t getTonePeriodCoarse(size_t chan) const {
        return registers[1 + chan * 2];
    }
    inline void setTonePeriodFine(size_t chan, uint8_t period) {
        registers[0 + chan * 2] = period;
        mask[0 + chan * 2] = true;
    }
    inline void setTonePeriodCoarse(size_t chan, uint8_t period) {
        registers[1 + chan * 2] = period;
        mask[1 + chan * 2] = true;
    }
    inline void setTonePeriod(size_t chan, uint16_t period) {
        setTonePeriodCoarse(chan, period >> 8);
        setTonePeriodFine(chan, period & 0xff);
    }
    inline uint16_t getTonePeriod(size_t chan) const {
        return static_cast<uint16_t>(getTonePeriodCoarse(chan) << 8) | getTonePeriodFine(chan);
    }
    inline bool hasNoisePeriodSet() const {
        return mask[6];
    }
    inline uint8_t getNoisePeriod() const {
        return registers[6];
    }
    inline void setNoisePeriod(uint8_t period) {
        registers[6] = period;
        mask[6] = true;
    }
    inline bool hasMixerSet() const {
        return mask[7];
    }
    inline uint8_t getMixer() const {
        return registers[7];
    }
    inline void setMixer(uint8_t mixer) {
        registers[7] = mixer;
        mask[7] = true;
    }
    inline bool hasVolumeAndEnvModSet(size_t chan) const {
        return mask[8 + chan];
    }
    inline uint8_t getVolumeAndEnvMod(size_t chan) const {
        return registers[8 + chan];
    }
    inline void setVolumeAndEnvMod(size_t chan, uint8_t volume) {
        registers[8 + chan] = volume;
        mask[8 + chan] = true;
    }
    inline bool hasEnvelopePeriodSet() const {
        return mask[11] || mask[12];
    }
    inline uint8_t getEnvelopePeriodFine() const {
        return registers[11];
    }
    inline uint8_t getEnvelopePeriodCoarse() const {
        return registers[12];
    }
    inline void setEnvelopePeriodFine(uint8_t period) {
        registers[11] = period;
        mask[11] = true;
    }
    inline void setEnvelopePeriodCoarse(uint8_t period) {
        registers[12] = period;
        mask[12] = true;
    }
    inline void setEnvelopePeriod(uint16_t period) {
        setEnvelopePeriodCoarse(period >> 8);
        setEnvelopePeriodFine(period & 0xff);
    }
    inline uint16_t getEnvelopePeriod() const {
        return static_cast<uint16_t>(getEnvelopePeriodCoarse() << 8) | getEnvelopePeriodFine();
    }
    inline bool hasEnvelopeShapeSet() const {
        return mask[13];
    }
    inline uint8_t getEnvelopeShape() const {
        return registers[13];
    }
    inline void setEnvelopeShape(uint8_t shape) {
        registers[13] = shape;
        mask[13] = true;
    }
};

// TODO maybe depack this and store depacked data in a separate structure to view it in a more convenient way

struct PSGData {
    size_t frameStep = 1;
    std::vector<PSGRegsAYFrame> frames;
    // TODO frequency table or id or shared_ptr to it
};


} // namespace MoTool::uZX