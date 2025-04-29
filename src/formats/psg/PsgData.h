#pragma once

#include <JuceHeader.h>
#include "../../util/enumchoice.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

namespace MoTool::uZX {

template <typename T, size_t NREGS>
struct PsgDeltaBase {
    std::array<T, NREGS> registers {};
    std::array<bool, NREGS> mask {};

    constexpr PsgDeltaBase() = default;
    constexpr PsgDeltaBase(const PsgDeltaBase&) = default;
    constexpr PsgDeltaBase(PsgDeltaBase&&) = default;
    constexpr PsgDeltaBase& operator=(const PsgDeltaBase&) = default;
    constexpr PsgDeltaBase& operator=(PsgDeltaBase&&) = default;
    constexpr PsgDeltaBase(std::array<uint8_t, NREGS>&& regs, std::array<bool, NREGS>&& m)
        : registers(std::move(regs)), mask(std::move(m)) {}

    inline static constexpr size_t size() noexcept{
        return NREGS;
    }

    inline constexpr bool isEmpty() const {
        return mask == std::array<bool, 14> {false};
    }

    inline constexpr bool isSet(size_t reg) const {
        return mask[reg];
    }

    inline constexpr T getRaw(size_t index) const noexcept {
        return registers[index];
    }

    inline void constexpr clear() noexcept {
        mask.fill(false);
    }
};

struct RegTypeEnum {
    enum Enum {
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
    static inline constexpr std::string_view labels[] {
        "TonePeriodFineA",
        "TonePeriodCoarseA",
        "TonePeriodFineB",
        "TonePeriodCoarseB",
        "TonePeriodFineC",
        "TonePeriodCoarseC",
        "NoisePeriod",
        "Mixer",
        "VolumeA",
        "VolumeB",
        "VolumeC",
        "EnvelopePeriodFine",
        "EnvelopePeriodCoarse",
        "EnvelopeShape"
    };
};

using PsgRegType = MoTool::Util::EnumChoice<RegTypeEnum>;

struct PsgRegsFrame : public PsgDeltaBase<uint8_t, 14> {

    using PsgDeltaBase::PsgDeltaBase;

    // Helpers for common register access
    inline constexpr bool hasTonePeriodSet(size_t chan) const noexcept {
        return mask[PsgRegType::TonePeriodFineA + chan * 2] || mask[PsgRegType::TonePeriodCoarseA + chan * 2];
    }
    inline constexpr uint8_t getTonePeriodFine(size_t chan) const noexcept {
        return registers[PsgRegType::TonePeriodFineA + chan * 2];
    }
    inline constexpr uint8_t getTonePeriodCoarse(size_t chan) const noexcept {
        return registers[PsgRegType::TonePeriodCoarseA + chan * 2];
    }
    inline constexpr void setTonePeriodFine(size_t chan, uint8_t period) noexcept {
        registers[PsgRegType::TonePeriodFineA + chan * 2] = period;
        mask[PsgRegType::TonePeriodFineA + chan * 2] = true;
    }
    inline constexpr void setTonePeriodCoarse(size_t chan, uint8_t period) noexcept {
        registers[PsgRegType::TonePeriodCoarseA + chan * 2] = period;
        mask[PsgRegType::TonePeriodCoarseA + chan * 2] = true;
    }
    inline constexpr void setTonePeriod(size_t chan, uint16_t period) noexcept {
        setTonePeriodCoarse(chan, period >> 8);
        setTonePeriodFine(chan, period & 0xff);
    }
    inline constexpr uint16_t getTonePeriod(size_t chan) const noexcept {
        return static_cast<uint16_t>(getTonePeriodCoarse(chan) << 8) | getTonePeriodFine(chan);
    }
    inline constexpr bool hasNoisePeriodSet() const noexcept {
        return mask[PsgRegType::NoisePeriod];
    }
    inline constexpr uint8_t getNoisePeriod() const noexcept {
        return registers[PsgRegType::NoisePeriod];
    }
    inline constexpr void setNoisePeriod(uint8_t period) noexcept {
        registers[PsgRegType::NoisePeriod] = period;
        mask[PsgRegType::NoisePeriod] = true;
    }
    inline constexpr bool hasMixerSet() const noexcept {
        return mask[PsgRegType::Mixer];
    }
    inline constexpr uint8_t getMixer() const noexcept {
        return registers[PsgRegType::Mixer];
    }
    inline constexpr void setMixer(uint8_t mixer) noexcept {
        registers[PsgRegType::Mixer] = mixer;
        mask[PsgRegType::Mixer] = true;
    }
    inline constexpr bool getToneOn(size_t chan) const noexcept {
        return !(registers[PsgRegType::Mixer] & (1 << chan));  // inverted
    }
    inline constexpr void setToneOn(size_t chan, bool on) noexcept {
        if (!on) {
            // DBG("registers[PsgRegType::Mixer] = " << registers[PsgRegType::Mixer] << "|" << (1 << chan));
            registers[PsgRegType::Mixer] |= (1 << chan);
        } else {
            // DBG("registers[PsgRegType::Mixer] = " << registers[PsgRegType::Mixer] << "&" << ~(1 << chan));
            registers[PsgRegType::Mixer] &= ~(1 << chan);
        }
        // DBG("setToneOn " << chan << " " << (on? "on" : "off") << " " << registers[PsgRegType::Mixer]);
        mask[PsgRegType::Mixer] = true;
    }
    inline constexpr bool getNoiseOn(size_t chan) const noexcept {
        return !(registers[PsgRegType::Mixer] & (0x08 << chan));  // inverted
    }
    inline constexpr void setNoiseOn(size_t chan, bool on) noexcept {
        if (!on) {  // inverted
            registers[PsgRegType::Mixer] |= (0x08 << chan);
        } else {
            registers[PsgRegType::Mixer] &= ~(0x08 << chan);
        }
        mask[PsgRegType::Mixer] = true;
    }
    inline constexpr bool hasVolumeOrEnvModSet(size_t chan) const noexcept {
        return mask[PsgRegType::VolumeA + chan];
    }
    inline constexpr bool hasVolumeSet(size_t chan) const noexcept {
        return mask[PsgRegType::VolumeA + chan] && (registers[PsgRegType::VolumeA + chan] & 0xF);
    }
    inline constexpr uint8_t getVolumeAndEnvMod(size_t chan) const noexcept {
        return registers[PsgRegType::VolumeA + chan];
    }
    inline constexpr bool getEnvMod(size_t chan) const noexcept {
        return registers[PsgRegType::VolumeA + chan] & 0x10;
    }
    inline constexpr uint8_t getVolume(size_t chan) const noexcept {
        return registers[PsgRegType::VolumeA + chan] & 0x0F;
    }
    inline constexpr void setVolumeAndEnvMod(size_t chan, uint8_t volume, bool envMod) noexcept {
        registers[PsgRegType::VolumeA + chan] = (volume & 0x0F) | (envMod ? 0x10 : 0);
        mask[PsgRegType::VolumeA + chan] = true;
    }
    inline constexpr void setEnvMod(size_t chan, bool envMod) noexcept {
        registers[PsgRegType::VolumeA + chan] = (registers[PsgRegType::VolumeA + chan] & 0x0F) | (envMod ? 0x10 : 0);
        mask[PsgRegType::VolumeA + chan] = true;
    }
    inline constexpr void setVolume(size_t chan, uint8_t volume) noexcept {
        registers[PsgRegType::VolumeA + chan] = (registers[PsgRegType::VolumeA + chan] & 0x10) | (volume & 0x0F);
        mask[PsgRegType::VolumeA + chan] = true;
    }
    inline constexpr bool hasEnvelopePeriodSet() const noexcept {
        return mask[PsgRegType::EnvelopePeriodFine] || mask[PsgRegType::EnvelopePeriodCoarse];
    }
    inline constexpr uint8_t getEnvelopePeriodFine() const noexcept {
        return registers[PsgRegType::EnvelopePeriodFine];
    }
    inline constexpr uint8_t getEnvelopePeriodCoarse() const noexcept {
        return registers[PsgRegType::EnvelopePeriodCoarse];
    }
    inline constexpr void setEnvelopePeriodFine(uint8_t period) noexcept {
        registers[PsgRegType::EnvelopePeriodFine] = period;
        mask[PsgRegType::EnvelopePeriodFine] = true;
    }
    inline constexpr void setEnvelopePeriodCoarse(uint8_t period) noexcept {
        registers[PsgRegType::EnvelopePeriodCoarse] = period;
        mask[PsgRegType::EnvelopePeriodCoarse] = true;
    }
    inline constexpr void setEnvelopePeriod(uint16_t period) noexcept {
        setEnvelopePeriodCoarse(period >> 8);
        setEnvelopePeriodFine(period & 0xff);
    }
    inline constexpr uint16_t getEnvelopePeriod() const noexcept {
        return static_cast<uint16_t>(getEnvelopePeriodCoarse() << 8) | getEnvelopePeriodFine();
    }
    inline constexpr bool hasEnvelopeShapeSet() const noexcept {
        return mask[PsgRegType::EnvelopeShape];
    }
    inline constexpr uint8_t getEnvelopeShape() const noexcept {
        return registers[PsgRegType::EnvelopeShape];
    }
    inline constexpr void setEnvelopeShape(uint8_t shape) noexcept {
        registers[PsgRegType::EnvelopeShape] = shape;
        mask[PsgRegType::EnvelopeShape] = true;
    }
    inline constexpr void clear() noexcept {
        std::fill(mask.begin(), mask.end(), false);
    }

    // Do not forget to clear before updating
    inline constexpr void update(const PsgRegsFrame& other) noexcept {
        for (size_t i = 0; i < size(); ++i) {
            if (other.mask[i]) {
                registers[i] = other.registers[i];
                mask[i] = true;
            }
        }
    }

    inline constexpr bool matches(const PsgRegsFrame& other) const noexcept {
        for (size_t i = 0; i < size(); ++i) {
            if ((mask[i] || other.mask[i]) && registers[i] != other.registers[i]) {
                return false;
            }
        }
        return true;
    }

    void debugPrint() const noexcept {
        for (size_t i = 0; i < size(); ++i) {
            DBG(std::string(static_cast<PsgRegType>(static_cast<int>(i)).getLabel()) << ": " << static_cast<int>(registers[i]) << " " << (mask[i] ? "true" : "false"));
        }
    }

    void debugPrintSet() const noexcept {
        for (size_t i = 0; i < size(); ++i) {
            if (!mask[i]) continue;
            DBG(std::string(static_cast<PsgRegType>(static_cast<int>(i)).getLabel()) << ": " << static_cast<int>(registers[i]));
        }
    }
};

struct PsgData {
    struct Options {
        double frameRate = 50;
        size_t playRate = 1;
        // bool usesRetriggers = false;
        // TODO frequency table or id or shared_ptr to it
    };

    std::vector<PsgRegsFrame> frames;
    Options options;

    void clear() {
        frames.clear();
    }

    size_t getLengthMachineFrames() const noexcept {
        return frames.size() * options.playRate;
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