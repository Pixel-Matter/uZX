#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace MoTool::uZX {

template <typename T, std::size_t NREGS>
struct PsgDeltaBase {
    std::array<T, NREGS> registers {};
    std::array<bool, NREGS> mask {};

    constexpr PsgDeltaBase() = default;
    constexpr PsgDeltaBase(const PsgDeltaBase&) = default;
    constexpr PsgDeltaBase(PsgDeltaBase&&) = default;
    constexpr PsgDeltaBase& operator=(const PsgDeltaBase&) = default;
    constexpr PsgDeltaBase& operator=(PsgDeltaBase&&) = default;
    constexpr PsgDeltaBase(std::array<T, NREGS>&& regs, std::array<bool, NREGS>&& m)
        : registers(std::move(regs)), mask(std::move(m)) {}

    inline static constexpr std::size_t size() noexcept{
        return NREGS;
    }

    inline constexpr bool isEmpty() const {
        return mask == std::array<bool, NREGS> {};
    }

    inline constexpr bool isSet(std::size_t reg) const {
        return mask[reg];
    }

    inline constexpr T getRaw(std::size_t index) const noexcept {
        return registers[index];
    }

    inline void constexpr clear() noexcept {
        mask.fill(false);
    }
};

struct RegTypeEnum {
    enum Enum : std::size_t {
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
};

/** AY register bytes and a mask identifying the writes for a single chip frame. */
struct PsgRegisterFrame : PsgDeltaBase<std::uint8_t, 14> {
    using PsgDeltaBase::PsgDeltaBase;

    // Helpers for common register access
    inline constexpr bool hasTonePeriodSet(std::size_t chan) const noexcept {
        return mask[RegTypeEnum::TonePeriodFineA + chan * 2] || mask[RegTypeEnum::TonePeriodCoarseA + chan * 2];
    }
    inline constexpr std::uint8_t getTonePeriodFine(std::size_t chan) const noexcept {
        return registers[RegTypeEnum::TonePeriodFineA + chan * 2];
    }
    inline constexpr std::uint8_t getTonePeriodCoarse(std::size_t chan) const noexcept {
        return registers[RegTypeEnum::TonePeriodCoarseA + chan * 2];
    }
    inline constexpr void setTonePeriodFine(std::size_t chan, std::uint8_t period) noexcept {
        registers[RegTypeEnum::TonePeriodFineA + chan * 2] = period;
        mask[RegTypeEnum::TonePeriodFineA + chan * 2] = true;
    }
    inline constexpr void setTonePeriodCoarse(std::size_t chan, std::uint8_t period) noexcept {
        registers[RegTypeEnum::TonePeriodCoarseA + chan * 2] = period;
        mask[RegTypeEnum::TonePeriodCoarseA + chan * 2] = true;
    }
    inline constexpr void setTonePeriod(std::size_t chan, std::uint16_t period) noexcept {
        setTonePeriodCoarse(chan, static_cast<std::uint8_t>(period >> 8));
        setTonePeriodFine(chan, static_cast<std::uint8_t>(period & 0xff));
    }
    inline constexpr std::uint16_t getTonePeriod(std::size_t chan) const noexcept {
        return static_cast<std::uint16_t>(getTonePeriodCoarse(chan) << 8) | getTonePeriodFine(chan);
    }
    inline constexpr bool hasNoisePeriodSet() const noexcept {
        return mask[RegTypeEnum::NoisePeriod];
    }
    inline constexpr std::uint8_t getNoisePeriod() const noexcept {
        return registers[RegTypeEnum::NoisePeriod];
    }
    inline constexpr void setNoisePeriod(std::uint8_t period) noexcept {
        registers[RegTypeEnum::NoisePeriod] = period;
        mask[RegTypeEnum::NoisePeriod] = true;
    }
    inline constexpr bool hasMixerSet() const noexcept {
        return mask[RegTypeEnum::Mixer];
    }
    inline constexpr std::uint8_t getMixer() const noexcept {
        return registers[RegTypeEnum::Mixer];
    }
    inline constexpr void setMixer(std::uint8_t mixer) noexcept {
        registers[RegTypeEnum::Mixer] = mixer;
        mask[RegTypeEnum::Mixer] = true;
    }
    inline constexpr bool getToneOn(std::size_t chan) const noexcept {
        return !(registers[RegTypeEnum::Mixer] & (1 << chan));  // inverted
    }
    inline constexpr void setToneOn(std::size_t chan, bool on) noexcept {
        if (!on) {
            registers[RegTypeEnum::Mixer] |= (1 << chan);
        } else {
            registers[RegTypeEnum::Mixer] &= static_cast<std::uint8_t>(~(1u << chan));
        }
        mask[RegTypeEnum::Mixer] = true;
    }
    inline constexpr bool getNoiseOn(std::size_t chan) const noexcept {
        return !(registers[RegTypeEnum::Mixer] & (0x08 << chan));  // inverted
    }
    inline constexpr void setNoiseOn(std::size_t chan, bool on) noexcept {
        if (!on) {  // inverted
            registers[RegTypeEnum::Mixer] |= (0x08 << chan);
        } else {
            registers[RegTypeEnum::Mixer] &= static_cast<std::uint8_t>(~(0x08u << chan));
        }
        mask[RegTypeEnum::Mixer] = true;
    }
    inline constexpr bool hasVolumeOrEnvModSet(std::size_t chan) const noexcept {
        return mask[RegTypeEnum::VolumeA + chan];
    }
    inline constexpr bool hasVolumeSet(std::size_t chan) const noexcept {
        return mask[RegTypeEnum::VolumeA + chan] && (registers[RegTypeEnum::VolumeA + chan] & 0xF);
    }
    inline constexpr std::uint8_t getVolumeAndEnvMod(std::size_t chan) const noexcept {
        return registers[RegTypeEnum::VolumeA + chan];
    }
    inline constexpr bool getEnvMod(std::size_t chan) const noexcept {
        return registers[RegTypeEnum::VolumeA + chan] & 0x10;
    }
    inline constexpr std::uint8_t getVolume(std::size_t chan) const noexcept {
        return registers[RegTypeEnum::VolumeA + chan] & 0x0F;
    }
    inline constexpr void setVolumeAndEnvMod(std::size_t chan, std::uint8_t volume, bool envMod) noexcept {
        registers[RegTypeEnum::VolumeA + chan] = (volume & 0x0F) | (envMod ? 0x10 : 0);
        mask[RegTypeEnum::VolumeA + chan] = true;
    }
    inline constexpr void setEnvMod(std::size_t chan, bool envMod) noexcept {
        registers[RegTypeEnum::VolumeA + chan] = (registers[RegTypeEnum::VolumeA + chan] & 0x0F) | (envMod ? 0x10 : 0);
        mask[RegTypeEnum::VolumeA + chan] = true;
    }
    inline constexpr void setVolume(std::size_t chan, std::uint8_t volume) noexcept {
        registers[RegTypeEnum::VolumeA + chan] = (registers[RegTypeEnum::VolumeA + chan] & 0x10) | (volume & 0x0F);
        mask[RegTypeEnum::VolumeA + chan] = true;
    }
    inline constexpr bool hasEnvelopePeriodSet() const noexcept {
        return mask[RegTypeEnum::EnvelopePeriodFine] || mask[RegTypeEnum::EnvelopePeriodCoarse];
    }
    inline constexpr std::uint8_t getEnvelopePeriodFine() const noexcept {
        return registers[RegTypeEnum::EnvelopePeriodFine];
    }
    inline constexpr std::uint8_t getEnvelopePeriodCoarse() const noexcept {
        return registers[RegTypeEnum::EnvelopePeriodCoarse];
    }
    inline constexpr void setEnvelopePeriodFine(std::uint8_t period) noexcept {
        registers[RegTypeEnum::EnvelopePeriodFine] = period;
        mask[RegTypeEnum::EnvelopePeriodFine] = true;
    }
    inline constexpr void setEnvelopePeriodCoarse(std::uint8_t period) noexcept {
        registers[RegTypeEnum::EnvelopePeriodCoarse] = period;
        mask[RegTypeEnum::EnvelopePeriodCoarse] = true;
    }
    inline constexpr void setEnvelopePeriod(std::uint16_t period) noexcept {
        setEnvelopePeriodCoarse(static_cast<std::uint8_t>(period >> 8));
        setEnvelopePeriodFine(static_cast<std::uint8_t>(period & 0xff));
    }
    inline constexpr std::uint16_t getEnvelopePeriod() const noexcept {
        return static_cast<std::uint16_t>(getEnvelopePeriodCoarse() << 8) | getEnvelopePeriodFine();
    }
    inline constexpr bool hasEnvelopeShapeSet() const noexcept {
        return mask[RegTypeEnum::EnvelopeShape];
    }
    inline constexpr std::uint8_t getEnvelopeShape() const noexcept {
        return registers[RegTypeEnum::EnvelopeShape];
    }
    inline constexpr void setEnvelopeShape(std::uint8_t shape) noexcept {
        registers[RegTypeEnum::EnvelopeShape] = shape;
        mask[RegTypeEnum::EnvelopeShape] = true;
    }
    inline constexpr void clear() noexcept {
        std::fill(mask.begin(), mask.end(), false);
    }

    // Do not forget to clear before updating
    inline constexpr void update(const PsgRegisterFrame& other) noexcept {
        for (std::size_t i = 0; i < size(); ++i) {
            if (other.mask[i]) {
                registers[i] = other.registers[i];
                mask[i] = true;
            }
        }
    }

    inline constexpr bool matches(const PsgRegisterFrame& other) const noexcept {
        for (std::size_t i = 0; i < size(); ++i) {
            if ((mask[i] || other.mask[i]) && registers[i] != other.registers[i]) {
                return false;
            }
        }
        return true;
    }

};

}  // namespace MoTool::uZX
