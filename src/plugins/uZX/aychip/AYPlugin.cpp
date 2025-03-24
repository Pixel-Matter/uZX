
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
    staticParams.clockValue.referTo(IDs::clock, "Clock frequncy", {0.894887, 2.0, 0.01}, 1.7734, "MHz");
    staticParams.chipTypeValue.referTo(IDs::chip, "Chip type", {AYInterface::TypeEnum::AY, AYInterface::TypeEnum::YM}, AYInterface::TypeEnum::AY, {});
}

AYChipPlugin::~AYChipPlugin() {
    notifyListenersOfDeletion();
}

const char* AYChipPlugin::xmlTypeName = "aychip";

void AYChipPlugin::valueTreeChanged() {
    te::Plugin::valueTreeChanged();
}

void AYChipPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    if (v == state && (id == IDs::clock || id == IDs::chip)) {
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
    te::copyPropertiesToCachedValues(v, staticParams.chipTypeValue.value, staticParams.clockValue.value);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

std::unique_ptr<te::Plugin::EditorComponent> AYChipPlugin::createEditor() {
    return std::make_unique<AYPluginEditor>(*this);
}

//==============================================================================

} // namespace MoTool::uZX


namespace juce {

template<>
struct VariantConverter<MoTool::uZX::AYInterface::ChipType> {
    static MoTool::uZX::AYInterface::ChipType fromVar(const var& v) {
        if (v == "YM")
            return MoTool::uZX::AYInterface::TypeEnum::YM;
        else
            return MoTool::uZX::AYInterface::TypeEnum::AY;
    }

    static var toVar(MoTool::uZX::AYInterface::ChipType ct) {
        if (ct == MoTool::uZX::AYInterface::TypeEnum::YM)
            return "YM";
        else
            return "AY";
    }
};

}
