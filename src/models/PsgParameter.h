#pragma once

// #include <JuceHeader.h>

#include "../formats/psg/PsgData.h"
#include "../util/enumchoice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace MoTool {

enum class ScaleType : unsigned short {
    Linear = 0,
    Log,
    ReciprocalLog
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
        "A: Volume",
        "B: Volume",
        "C: Volume",
        "A: Tone Period",
        "B: Tone Period",
        "C: Tone Period",
        "A: Tone Is On",
        "B: Tone Is On",
        "C: Tone Is On",
        "A: Noise Is On",
        "B: Noise Is On",
        "C: Noise Is On",
        "A: Envelope Is On",
        "B: Envelope Is On",
        "C: Envelope Is On",
        "Noise Period",
        "Envelope: Period",
        "Envelope: Shape",
        "A: Retrigger Tone",
        "B: Retrigger Tone",
        "C: Retrigger Tone",
        "Envelope: Retrigger",
    };
};

class PsgParamType : public Util::EnumChoice<PsgParamTypeEnum> {
public:
    using Util::EnumChoice<PsgParamTypeEnum>::EnumChoice;

    ParameterScale inline getScale() const noexcept {
        switch (asEnum()) {
            case VolumeA:
            case VolumeB:
            case VolumeC:
                return {0, 15, ScaleType::Linear, {}};
            case TonePeriodA:
            case TonePeriodB:
            case TonePeriodC:
                return {0, 4095, ScaleType::ReciprocalLog, {}};
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
                return {0, 1, ScaleType::Linear, {}};
            case NoisePeriod:
                return {0, 31, ScaleType::Linear, {}};
            case EnvelopePeriod:
                return {0, 256, ScaleType::ReciprocalLog, {}};
            case EnvelopeShape:
                return {0, 15, ScaleType::Linear, {}};
            default:
                return {0, 0, ScaleType::Linear, {}};
        }
    }

    /** Convert raw parameter value to normalized 0-1 range based on scale type */
    float valueToNormalized(int value) const noexcept {
        const auto scale = getScale();
        const float range = static_cast<float>(scale.end - scale.start);
        if (range <= 0.0f) return 0.0f;

        const float v = static_cast<float>(value - scale.start);

        switch (scale.type) {
            case ScaleType::Linear:
                return v / range;
            case ScaleType::Log:
                return std::log(v + 1.0f) / std::log(range + 1.0f);
            case ScaleType::ReciprocalLog:
                // Inverted log: high values (low frequency) → bottom, low values (high frequency) → top
                return 1.0f - std::log(v + 1.0f) / std::log(range + 1.0f);
            default:
                return v / range;
        }
    }

    /** Convert normalized 0-1 value back to raw parameter value based on scale type */
    int normalizedToValue(float normalized) const noexcept {
        const auto scale = getScale();
        const float range = static_cast<float>(scale.end - scale.start);
        if (range <= 0.0f) return scale.start;

        const float n = juce::jlimit(0.0f, 1.0f, normalized);
        float v;

        switch (scale.type) {
            case ScaleType::Linear:
                v = n * range;
                break;
            case ScaleType::Log:
                v = std::exp(n * std::log(range + 1.0f)) - 1.0f;
                break;
            case ScaleType::ReciprocalLog:
                // Inverse of reciprocal log
                v = std::exp((1.0f - n) * std::log(range + 1.0f)) - 1.0f;
                break;
            default:
                v = n * range;
                break;
        }

        return scale.start + static_cast<int>(std::round(v));
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
                set(PsgParamType::EnvelopeIsOnA + int(i), regs.getEnvMod(i));
                // NOTE PSG clearing must be a separate process
                // if (!regs.getEnvMod(i)) {
                    // volume set only if envelope is off
                    set(PsgParamType::VolumeA + int(i), regs.getVolume(i));
                // }
            }
            if (regs.hasTonePeriodSet(i)) {
                set(PsgParamType::TonePeriodA + int(i), regs.getTonePeriod(i));
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
            if (isSet(PsgParamType::EnvelopeIsOnA + int(i)) || isSet(PsgParamType::VolumeA + int(i))) {
                regs.setVolumeAndEnvMod(size_t(i), uint8(getRaw(PsgParamType::VolumeA + int(i))),
                                                         getRaw(PsgParamType::EnvelopeIsOnA + int(i)));
            }
        }
        for (size_t i = 0; i < 3; ++i) {
            if (isSet(PsgParamType::TonePeriodA + int(i))) {
                regs.setTonePeriod(i, getRaw(PsgParamType::TonePeriodA + int(i)));
            }
        }
        // Set all mixer params regardless of individual mask
        bool mixerSet = false;
        for (size_t i = 0; i < 3; ++i) {
            mixerSet = mixerSet || isSet(PsgParamType::ToneIsOnA + int(i)) || isSet(PsgParamType::NoiseIsOnA + int(i));
        }
        if (mixerSet) {
            for (size_t i = 0; i < 3; ++i) {
                regs.setToneOn (i, getRaw(PsgParamType::ToneIsOnA + int(i)));
                regs.setNoiseOn(i, getRaw(PsgParamType::NoiseIsOnA + int(i)));
            }
        }
        // Noise and env para
        if (isSet(PsgParamType::NoisePeriod)) {
            regs.setNoisePeriod(static_cast<uint8>(getRaw(PsgParamType::NoisePeriod)));
        }
        if (isSet(PsgParamType::EnvelopePeriod)) {
            regs.setEnvelopePeriod(getRaw(PsgParamType::EnvelopePeriod));
        }
        // Write envelope shape if shape changed OR retrigger=1
        if (isSet(PsgParamType::EnvelopeShape) ||
            (isSet(PsgParamType::RetriggerEnvelope) && getRaw(PsgParamType::RetriggerEnvelope) == 1)) {
            regs.setEnvelopeShape(static_cast<uint8>(getRaw(PsgParamType::EnvelopeShape)));
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
        PsgParamType::forEach([this](auto enumValue) {
            auto type = PsgParamType(enumValue);
            DBG(std::string(type.getLabel()) << ": " << values[static_cast<size_t>(type)]);
        });
    }

    void debugPrintSet() const noexcept {
        PsgParamType::forEach([this](auto enumValue) {
            auto type = PsgParamType(enumValue);
            if (isSet(type)) {
                DBG(std::string(type.getLabel()) << ": " << values[static_cast<size_t>(type)]);
            }
        });
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
        unSet(PsgParamType::ToneIsOnA);
        unSet(PsgParamType::ToneIsOnB);
        unSet(PsgParamType::ToneIsOnC);
        unSet(PsgParamType::NoiseIsOnA);
        unSet(PsgParamType::NoiseIsOnB);
        unSet(PsgParamType::NoiseIsOnC);
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
    std::array<bool,     static_cast<size_t>(PsgParamType::size())> masks {};
};


}  // namespace MoTool