#pragma once

// #include <JuceHeader.h>

#include "../formats/psg/PsgData.h"
#include "../util/enumchoice.h"
#include "juce_core/juce_core.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>

namespace MoTool {

enum class ScaleType : uint8 {
    LinearScale = 0,
    LogScale,
    ReverseLogScale
};

struct ParameterScale {
    int start;
    int end;
    ScaleType type;
    StringArray labels;
};

struct PsgParamTypeEnum {
    enum Enum {
        VolumeA = 0,
        VolumeB,
        VolumeC,
        TonePeriodA,
        TonePeriodB,
        TonePeriodC,
        ToneIsOnA,
        ToneIsOnB,
        ToneIsOnC,
        NoiseIsOnA,
        NoiseIsOnB,
        NoiseIsOnC,
        EnvelopeIsOnA,
        EnvelopeIsOnB,
        EnvelopeIsOnC,
        NoisePeriod,
        EnvelopePeriod,
        EnvelopeShape,
        RetriggerToneA,
        RetriggerToneB,
        RetriggerToneC,
        RetriggerEnvelope,
    };

    static inline constexpr std::string_view labels[] {
        "Volume A",
        "Volume B",
        "Volume C",
        "Tone Period A",
        "Tone Period B",
        "Tone Period C",
        "Tone Is On A",
        "Tone Is On B",
        "Tone Is On C",
        "Noise Is On A",
        "Noise Is On B",
        "Noise Is On C",
        "Envelope Is On A",
        "Envelope Is On B",
        "Envelope Is On C",
        "Noise Period",
        "Envelope Period",
        "Envelope Shape",
        "Retrigger Tone A",
        "Retrigger Tone B",
        "Retrigger Tone C",
        "Retrigger Envelope",
    };
};

class PsgParamType : public Util::EnumChoice<PsgParamTypeEnum> {
public:
    using Util::EnumChoice<PsgParamTypeEnum>::EnumChoice;

    NormalisableRange<size_t> inline constexpr getRange() const noexcept {
        switch (asEnum()) {
            case VolumeA:
            case VolumeB:
            case VolumeC:
                return NormalisableRange<size_t>(0, 15);
            case TonePeriodA:
            case TonePeriodB:
            case TonePeriodC:
                return NormalisableRange<size_t>(0, 4095);
            case ToneIsOnA:
            case ToneIsOnB:
            case ToneIsOnC:
            case NoiseIsOnA:
            case NoiseIsOnB:
            case NoiseIsOnC:
            case EnvelopeIsOnA:
            case EnvelopeIsOnB:
            case EnvelopeIsOnC:
            case RetriggerToneA:
            case RetriggerToneB:
            case RetriggerToneC:
            case RetriggerEnvelope:
                return NormalisableRange<size_t>(0, 1);
            case NoisePeriod:
                return NormalisableRange<size_t>(0, 31);
            case EnvelopePeriod:
                return NormalisableRange<size_t>(0, 4095);
            case EnvelopeShape:
                return NormalisableRange<size_t>(0, 15);
            default:
                return NormalisableRange<size_t>(0, 0);
        }
    }
};

class PsgParamFrameData {
public:

    constexpr PsgParamFrameData() noexcept = default;
    constexpr PsgParamFrameData(const PsgParamFrameData&) = default;
    constexpr PsgParamFrameData(PsgParamFrameData&&) = default;
    constexpr PsgParamFrameData& operator=(const PsgParamFrameData&) = default;
    constexpr PsgParamFrameData& operator=(PsgParamFrameData&&) = default;
    constexpr ~PsgParamFrameData() = default;

    constexpr PsgParamFrameData(std::initializer_list<std::pair<PsgParamType, uint16_t>> params) noexcept {
        for (const auto& [type, value] : params) {
            set(type, value);
        }
    }

    constexpr PsgParamFrameData(const std::vector<std::pair<PsgParamType, uint16_t>>& params) noexcept {
        for (const auto& [type, value] : params) {
            set(type, value);
        }
    }

    constexpr PsgParamFrameData(const std::vector<std::tuple<PsgParamType, uint16_t, bool>>& params) noexcept {
        for (const auto& [type, value, isSet] : params) {
            values[static_cast<size_t>(type)] = value;
            masks[static_cast<size_t>(type)] = isSet;
        }
    }

    explicit constexpr PsgParamFrameData(const uZX::PsgRegsFrame& regs) noexcept {
        for (size_t i = 0; i < 3; ++i) {
            if (regs.hasVolumeOrEnvModSet(i)) {
                set(PsgParamType(int(PsgParamType::EnvelopeIsOnA) + int(i)), regs.getEnvMod(i));
                // NOTE PSG clearing must be a separate process
                // if (!regs.getEnvMod(i)) {
                    // volume set only if envelope is off
                    set(PsgParamType(int(PsgParamType::VolumeA) + int(i)), regs.getVolume(i));
                // }
            }
            if (regs.hasTonePeriodSet(i)) {
                set(PsgParamType(int(PsgParamType::TonePeriodA) + int(i)), regs.getTonePeriod(i));
            }
        }
        if (regs.hasMixerSet()) {
            set(PsgParamType::ToneIsOnA,  regs.getToneOn(0));
            set(PsgParamType::ToneIsOnB,  regs.getToneOn(1));
            set(PsgParamType::ToneIsOnC,  regs.getToneOn(2));
            set(PsgParamType::NoiseIsOnA, regs.getNoiseOn(0));
            set(PsgParamType::NoiseIsOnB, regs.getNoiseOn(1));
            set(PsgParamType::NoiseIsOnC, regs.getNoiseOn(2));
            // DBG("Mixer from PsgRegsFrame (" << static_cast<int>(regs.getMixer()) << ") set");
        }
        if (regs.hasNoisePeriodSet()) {
            set(PsgParamType::NoisePeriod, regs.getNoisePeriod());
        }
        if (regs.hasEnvelopePeriodSet()) {
            set(PsgParamType::EnvelopePeriod, regs.getEnvelopePeriod());
        }
        if (regs.hasEnvelopeShapeSet()) {
            set(PsgParamType::EnvelopeShape, regs.getEnvelopeShape());
        }
    }

    inline static constexpr size_t size() noexcept {
        return PsgParamType::size();
    }

    constexpr void updateRegisters(uZX::PsgRegsFrame& regs) const noexcept {
        // per-tone-channel parameters
        for (size_t i = 0; i < 3; ++i) {
            // NOTE PSG clearing must be a separate process
            // if (masks[size_t(PsgParamType::EnvelopeIsOnA) + i] && values[size_t(PsgParamType::EnvelopeIsOnA) + i]) {
            //     // if env is set, volume is 0
            //     regs.setVolumeAndEnvMod(i, 0, values[size_t(PsgParamType::EnvelopeIsOnA) + i]);
            // } else
            if (masks[size_t(PsgParamType::EnvelopeIsOnA) + i] || masks[size_t(PsgParamType::VolumeA) + i]) {
                // DBG("Setting volume/env chan " << i << ": " << values[size_t(PsgParamType::VolumeA) + i] << ", " << values[size_t(PsgParamType::EnvelopeIsOnA) + i]);
                regs.setVolumeAndEnvMod(i, uint8(values[size_t(PsgParamType::VolumeA) + i]), values[size_t(PsgParamType::EnvelopeIsOnA) + i]);
            }
        }
        for (size_t i = 0; i < 3; ++i) {
            if (masks[size_t(PsgParamType::TonePeriodA) + i]) {
                // DBG("Setting tone period chan " << i << ": " << values[size_t(PsgParamType::TonePeriodA) + i]);
                regs.setTonePeriod(i, values[size_t(PsgParamType::TonePeriodA) + i]);
            }
        }
        // Set all mixer params regardless of individual mask
        bool mixerSet = false;
        for (size_t i = 0; i < 3; ++i) {
            mixerSet = mixerSet || masks[size_t(PsgParamType::ToneIsOnA) + i] || masks[size_t(PsgParamType::NoiseIsOnA) + i];
        }
        if (mixerSet) {
            for (size_t i = 0; i < 3; ++i) {
                // DBG("Setting mixer chan " << i << ": t="
                //     << values[size_t(PsgParamType::ToneIsOnA) + i] << ", n="
                //     << values[size_t(PsgParamType::NoiseIsOnA) + i]);
                regs.setToneOn(i, values[size_t(PsgParamType::ToneIsOnA) + i]);
                regs.setNoiseOn(i, values[size_t(PsgParamType::NoiseIsOnA) + i]);
            }
            // DBG("Mixer set to " << regs.getMixer());
        }
        // Noise and env para
        if (masks[size_t(PsgParamType::NoisePeriod)]) {
            // DBG("Setting noise period: " << values[size_t(PsgParamType::NoisePeriod)]);
            regs.setNoisePeriod(static_cast<uint8>(values[size_t(PsgParamType::NoisePeriod)]));
        }
        if (masks[size_t(PsgParamType::EnvelopePeriod)]) {
            // DBG("Setting envelope period: " << values[size_t(PsgParamType::EnvelopePeriod)]);
            regs.setEnvelopePeriod(values[size_t(PsgParamType::EnvelopePeriod)]);
        }
        if (masks[size_t(PsgParamType::EnvelopeShape)]) {
            // DBG("Setting envelope shape: " << values[size_t(PsgParamType::EnvelopeShape)]);
            regs.setEnvelopeShape(static_cast<uint8>(values[size_t(PsgParamType::EnvelopeShape)]));
        }
        // TODO retrigger tone
        // if (masks[size_t(PsgParamType::RetriggerToneA)]) {
        //     DBG("Setting retrigger tone A: " << values[size_t(PsgParamType::RetriggerToneA)]);
        //     regs.setRetriggerTone(0, values[size_t(PsgParamType::RetriggerToneA)]);
        // }
    }

    constexpr uZX::PsgRegsFrame toRegisters() const noexcept {
        uZX::PsgRegsFrame regs;
        updateRegisters(regs);
        return regs;
    }

    constexpr void update(const uZX::PsgRegsFrame& regs) noexcept {
        update(PsgParamFrameData {regs});
    }

    constexpr void update(const PsgParamFrameData& data) noexcept {
        // track what params was changed actually, compare with current values
        // values = data.values;
        for (size_t i = 0; i < PsgParamType::size(); ++i) {
            masks[i] = data.masks[i] && data.values[i] != values[i];
            if (masks[i]) {
                values[i] = data.values[i];
            }
        }
    }

    void debugPrint() const noexcept {
        for (size_t i = 0; i < PsgParamType::size(); ++i) {
            DBG(std::string(static_cast<PsgParamType>(static_cast<int>(i)).getLabel()) << ": " << values[i] << " " << (masks[i] ? "true" : "false"));
        }
    }

    void debugPrintSet() const noexcept {
        for (size_t i = 0; i < PsgParamType::size(); ++i) {
            if (!masks[i]) continue;
            DBG(std::string(static_cast<PsgParamType>(static_cast<int>(i)).getLabel()) << ": " << values[i]);
        }
    }

    constexpr bool isEmpty() const noexcept {
        return !std::any_of(masks.begin(), masks.end() , [](bool mask) { return mask; });
    }

    inline constexpr std::optional<uint16_t> operator [](PsgParamType type) const noexcept {
        return masks[type] ? std::optional<uint16_t> {values[type]} : std::nullopt;
    }

    inline constexpr bool isSet(PsgParamType type) const noexcept {
        return masks[static_cast<size_t>(type)];
    }

    inline constexpr uint16_t& set(PsgParamType type, uint16_t value) noexcept {
        masks[static_cast<size_t>(type)] = true;
        values[static_cast<size_t>(type)] = value;
        return values[static_cast<size_t>(type)];
    }

    inline constexpr uint16_t getRaw(PsgParamType type) const noexcept {
        return values[static_cast<size_t>(type)];
    }

    inline constexpr void unSet(PsgParamType type) noexcept {
        masks[static_cast<size_t>(type)] = false;
    }

    inline constexpr void clear() noexcept {
        std::fill(masks.begin(), masks.end(), false);
    }

    inline constexpr void reset() noexcept {
        std::fill(masks.begin(), masks.end(), false);
        std::fill(values.begin(), values.end(), 0);
    }

    inline constexpr void resetMixer() noexcept {
        values[static_cast<size_t>(PsgParamType::ToneIsOnA)] = 1;
        values[static_cast<size_t>(PsgParamType::ToneIsOnB)] = 1;
        values[static_cast<size_t>(PsgParamType::ToneIsOnC)] = 1;
        values[static_cast<size_t>(PsgParamType::NoiseIsOnA)] = 1;
        values[static_cast<size_t>(PsgParamType::NoiseIsOnB)] = 1;
        values[static_cast<size_t>(PsgParamType::NoiseIsOnC)] = 1;
        masks[static_cast<size_t>(PsgParamType::ToneIsOnA)]  = false;
        masks[static_cast<size_t>(PsgParamType::ToneIsOnB)]  = false;
        masks[static_cast<size_t>(PsgParamType::ToneIsOnC)]  = false;
        masks[static_cast<size_t>(PsgParamType::NoiseIsOnA)] = false;
        masks[static_cast<size_t>(PsgParamType::NoiseIsOnB)] = false;
        masks[static_cast<size_t>(PsgParamType::NoiseIsOnC)] = false;
        // masks[static_cast<size_t>(PsgParamType::ToneIsOnA)] = true;
        // masks[static_cast<size_t>(PsgParamType::ToneIsOnB)] = true;
        // masks[static_cast<size_t>(PsgParamType::ToneIsOnC)] = true;
        // masks[static_cast<size_t>(PsgParamType::NoiseIsOnA)] = true;
        // masks[static_cast<size_t>(PsgParamType::NoiseIsOnB)] = true;
        // masks[static_cast<size_t>(PsgParamType::NoiseIsOnC)] = true;
    }

    constexpr std::vector<std::pair<PsgParamType, uint16_t>> getParams() const {
        std::vector<std::pair<PsgParamType, uint16_t>> result;
        result.reserve(std::size(values));
        for (size_t i = 0; i < std::size(values); ++i) {
            if (masks[i]) {
                result.emplace_back(static_cast<PsgParamType>(static_cast<int>(i)), values[i]);
            }
        }
        result.shrink_to_fit();
        return result;
    }

    friend class PsgParamFrame;
    friend class PsgParamsChangeTracker;

private:
    // not map or vector to avoid dynamic allocations
    std::array<uint16_t, static_cast<size_t>(PsgParamType::size())> values {};
    // TODO or use bitmask for 21 bits
    // sizeof(std::vector<bool>) is 24 bytes
    std::array<bool,     static_cast<size_t>(PsgParamType::size())> masks {};
};


}  // namespace MoTool