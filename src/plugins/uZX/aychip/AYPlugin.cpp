
#include <JuceHeader.h>
#include <utility>

#include "AYPlugin.h"
#include "AYPluginEditor.h"

namespace te = tracktion;

namespace MoTool::uZX {


//==============================================================================
AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
{
    // TODO staticParams.initialise();
    staticParams.clockValue.referTo(IDs::clock, "Clock frequncy", {0.894887, 2.0, 0.01}, 1.7734, "MHz");
    staticParams.chipTypeValue.referTo(IDs::chip, "Chip type", AYInterface::ChipType::getLabels(), AYInterface::TypeEnum::AY, {});
    staticParams.channelsLayoutValue.referTo(IDs::layout, "Channels layout", AYInterface::ChannelsLayout::getLabels(), AYInterface::LayoutEnum::ABC, {});
    staticParams.stereoWidthValue.referTo(IDs::stereo, "Stereo width", {0.0, 1.0, 0.01}, 0.5, {});
}

AYChipPlugin::~AYChipPlugin() {
    notifyListenersOfDeletion();
}

const char* AYChipPlugin::xmlTypeName = "aychip";

void AYChipPlugin::valueTreeChanged() {
    te::Plugin::valueTreeChanged();
}

void AYChipPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    // TODO staticParams.isParamProperty(id);
    if (v == state && (id == IDs::clock || id == IDs::chip || id == IDs::stereo)) {
        reset();
    }
    propertiesChanged();
    Plugin::valueTreePropertyChanged(v, id);
}

void AYChipPlugin::initialise(const te::PluginInitialisationInfo&) {
    reset();
}

void AYChipPlugin::deinitialise() {
    chip = nullptr;
}

void AYChipPlugin::midiPanic() {
    reset();
}

void AYChipPlugin::reset() {
    const ScopedLock sl(lock);
    if (chip == nullptr) {
        chip = std::make_unique<AyumiEmulator>(sampleRate, staticParams.clockValue * MHz, staticParams.chipTypeValue);
    } else {
        chip->reset(static_cast<int>(sampleRate), staticParams.clockValue * MHz, staticParams.chipTypeValue);
    }
    // TODO
    // chip->setChannelsLayout(staticParams.channelsLayoutValue);
    // chip->setStereoWidth(staticParams.stereoWidthValue);
    chip->setMasterVolume(0.6f);
    timeFromReset = 0.0;
    registers = {};
}

void AYChipPlugin::applyToBuffer(const te::PluginRenderContext& fc) noexcept {
    if (chip == nullptr || fc.destBuffer == nullptr || fc.bufferForMidiMessages == nullptr
        || !(fc.isPlaying || fc.isScrubbing || fc.isRendering)
    ) {
        return;
    }

    SCOPED_REALTIME_CHECK
    const ScopedLock sl(lock);

    te::clearChannels(*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

    // Process PSG regiser events, no midi notes on this low level
    int currentSample = 0;
    for (auto& m : *fc.bufferForMidiMessages) {
        // process up to this event
        const int timeSample = roundToInt(m.getTimeStamp() * sampleRate);
        if (timeSample - currentSample > 0) {
            chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample), fc.destBuffer->getWritePointer(1, currentSample),
                               static_cast<size_t>(timeSample - currentSample));
            currentSample = timeSample;
        }
        if (m.isNoteOn()) {
            // note based PSG-MIDI mapping is not used yet
            // but can be used with MPE
            // const int note = m.getNoteNumber();
            // const size_t reg = static_cast<size_t>(note - 60);
            // const unsigned char val = m.getVelocity();
            // chip->setRegister(reg, val);
            // DBG("setRegister(" << reg << ", " << val << ")");
        } else if (m.isController()) {
            const int ctrlNum = m.getControllerNumber();
            const int val = static_cast<unsigned char>(m.getControllerValue());
            size_t reg = 0;
            if (20 <= ctrlNum && ctrlNum < 34) {
                // coarse value
                reg = static_cast<size_t>(ctrlNum - 20);
                registers.registers[reg] = static_cast<unsigned char>((val << 4) | registers.registers[reg]);
                registers.mask[reg] = !registers.mask[reg];
                // DBG("register coarse " << reg << ", " << m.getControllerValue() << ", mask " << (regs.mask[reg] ? "on" : "off"));
            } else if (40 <= ctrlNum && ctrlNum < 54) {
                // fine value
                reg = static_cast<size_t>(ctrlNum - 40);
                registers.registers[reg] = static_cast<unsigned char>(val | registers.registers[reg]);
                registers.mask[reg] = !registers.mask[reg];
                // DBG("register fine " << reg << ", " << m.getControllerValue() << ", mask " << (regs.mask[reg] ? "on" : "off") << " reg is " << regs.registers[reg]);
            } else {
                jassertfalse;
            }
            if (!registers.mask[reg]) {
                chip->setRegister(reg, registers.registers[reg]);
                // DBG("setRegister(" << reg << ", " << regs.registers[reg] << ")");
                registers.registers[reg] = 0;
            }
        }
    }
    // process to the end of the block
    if (currentSample < fc.destBuffer->getNumSamples()) {
        chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample), fc.destBuffer->getWritePointer(1, currentSample),
                           static_cast<size_t>(fc.bufferNumSamples - currentSample),
                           /* removeDC = */ false
                        );
    }
    timeFromReset += (double) fc.destBuffer->getNumSamples() / sampleRate;
    // DBG("timeFromReset = " << timeFromReset);
}

void AYChipPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v) {
    // TODO staticParams.copyPropertiesToCachedValues(v);
    te::copyPropertiesToCachedValues(v,
        staticParams.chipTypeValue.value,
        staticParams.clockValue.value,
        staticParams.channelsLayoutValue.value,
        staticParams.stereoWidthValue.value);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

std::unique_ptr<te::Plugin::EditorComponent> AYChipPlugin::createEditor() {
    return std::make_unique<AYPluginEditor>(*this);
}

//==============================================================================

template <typename ChoiceType>
static ChoiceType choiceFromVar(const var& v) {
    // look for the first match from getLabels
    for (int i = 0; i <= static_cast<int>(ChoiceType::size()); ++i) {
        auto ct = ChoiceType::getLabelFor(static_cast<size_t>(i));
        if (v.toString().toStdString() == ct)
            return static_cast<ChoiceType>(i);
    }
    return static_cast<ChoiceType>(0);
}

template <typename ChoiceType>
static var choicetoVar(ChoiceType c) {
    const std::string_view label = c.getLabel();
    return String {label.data(), label.size()};
}

} // namespace MoTool::uZX


namespace juce {

using namespace MoTool::uZX;

template<>
struct VariantConverter<AYInterface::ChipType> {
    static AYInterface::ChipType fromVar(const var& v) {
        return choiceFromVar<AYInterface::ChipType>(v);
    }

    static var toVar(AYInterface::ChipType ct) {
        return choicetoVar(ct);
    }
};

template<>
struct VariantConverter<AYInterface::ChannelsLayout> {
    static AYInterface::ChannelsLayout fromVar(const var& v) {
        return choiceFromVar<AYInterface::ChannelsLayout>(v);
    }

    static var toVar(AYInterface::ChannelsLayout layout) {
        return choicetoVar(layout);
    }
};

} // namespace juce
