
#include "PsgCC.h"

namespace MoTool {

//==============================================================================
// PsgCC: the table both directions read
//==============================================================================
namespace {

struct PsgCCRow {
    PsgParamTypeEnum::Enum first;  // the parameter on `channel`
    int count;                     // 3: one for each of A, B and C on consecutive channels; 1: one for the chip
    int channel;
    MidiCCType controller;
    bool isPeriod;                 // split over `controller` and its fine pair

    bool has(PsgParamType param) const noexcept {
        return juce::isPositiveAndBelow(static_cast<int>(param) - static_cast<int>(first), count);
    }
};

constexpr PsgCCRow psgCcRows[] {
    {PsgParamType::VolumeA,           3, 0, MidiCCType::Volume,              false},
    {PsgParamType::TonePeriodA,       3, 0, MidiCCType::CC20PeriodCoarse,    true},
    {PsgParamType::ToneIsOnA,         3, 0, MidiCCType::GPB1ToneSwitch,      false},
    {PsgParamType::NoiseIsOnA,        3, 0, MidiCCType::GPB2NoiseSwitch,     false},
    {PsgParamType::EnvelopeIsOnA,     3, 0, MidiCCType::GPB3EnvSwitch,       false},
    {PsgParamType::RetriggerToneA,    3, 0, MidiCCType::GPB4RetriggerSwitch, false},
    {PsgParamType::NoisePeriod,       1, 3, MidiCCType::Breath,              false},
    {PsgParamType::EnvelopePeriod,    1, 3, MidiCCType::CC20PeriodCoarse,    true},
    {PsgParamType::EnvelopeShape,     1, 3, MidiCCType::SoundVariation,      false},
};

// MIDI pairs a coarse controller below 32 with the fine one 32 above it
constexpr int fineOffset = 32;
static_assert(static_cast<int>(MidiCCType::CC52PeriodFine) == static_cast<int>(MidiCCType::CC20PeriodCoarse) + fineOffset);

}  // namespace

PsgCC::Messages PsgCC::encode(PsgParamType param, int value) noexcept {
    for (const auto& row : psgCcRows) {
        if (!row.has(param))
            continue;
        const int channel = row.channel + static_cast<int>(param) - static_cast<int>(row.first);
        if (!row.isPeriod)
            return {{{{channel, row.controller, value}}}, 1};
        const auto fine = static_cast<MidiCCType>(static_cast<int>(row.controller) + fineOffset);
        return {{{{channel, row.controller, (value >> 7) & 0x7F}, {channel, fine, value & 0x7F}}}, 2};
    }
    jassertfalse;
    return {};
}

bool PsgCC::decode(int channel, int controller, int value, PsgParamFrameData& into) noexcept {
    for (const auto& row : psgCcRows) {
        if (!juce::isPositiveAndBelow(channel - row.channel, row.count))
            continue;
        const PsgParamType param {static_cast<int>(row.first) + channel - row.channel};
        const int coarse = static_cast<int>(row.controller);
        const auto raw = into.getRaw(param);
        if (controller == coarse)
            into.set(param, static_cast<uint16_t>(row.isPeriod ? (value << 7) | (raw & 0x7F) : value));
        else if (row.isPeriod && controller == coarse + fineOffset)
            into.set(param, static_cast<uint16_t>((raw & 0xFF80) | value));
        else
            continue;
        return true;
    }
    return false;
}

//===============================================================================
// PsgParamsMidiWriter class
//===============================================================================
void PsgParamsMidiWriter::write(double time, const PsgParamFrameData& data) {
    for (auto [type, value] : data.getParams())
        for (const auto& m : PsgCC::encode(type, value))
            sequence.addEvent(juce::MidiMessage::controllerEvent(channelNumber + m.channel, static_cast<int>(m.controller), m.value), time);
}

//===============================================================================
// PsgEncoder class
//===============================================================================
static constexpr bool isRetriggerTone(PsgParamType param) noexcept {
    return param == PsgParamType::RetriggerToneA || param == PsgParamType::RetriggerToneB
        || param == PsgParamType::RetriggerToneC;
}

void PsgEncoder::encode(const PsgParamFrameData& params, tracktion::MidiMessageArray& out, double time, int baseChannel) {
    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        const PsgParamType param {i};
        if (!params.isSet(param))
            continue;
        const auto value = params.getRaw(param);
        if (isRetriggerTone(param)) {
            if (value == 0)
                continue;
        } else {
            // A set envelope shape is a write that restarts the envelope, so it goes out with the same value too
            if (param != PsgParamType::EnvelopeShape && sent_.isSet(param) && sent_.getRaw(param) == value)
                continue;
            sent_.set(param, value);
        }
        for (const auto& m : PsgCC::encode(param, value))
            out.addMidiMessage(juce::MidiMessage::controllerEvent(baseChannel + m.channel, static_cast<int>(m.controller), m.value),
                               time, 0);
    }
}

}  // namespace MoTool
