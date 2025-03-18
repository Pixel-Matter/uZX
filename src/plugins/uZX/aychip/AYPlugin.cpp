
#include <JuceHeader.h>

#include "AYPlugin.h"

namespace te = tracktion;

namespace MoTool::uZX {


//==============================================================================
AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : Plugin(info)
{
    triggerAsyncUpdate();
}

AYChipPlugin::~AYChipPlugin() {
    notifyListenersOfDeletion();
}

const char* AYChipPlugin::xmlTypeName = "aychip";

void AYChipPlugin::valueTreeChanged() {
    triggerAsyncUpdate();
    te::Plugin::valueTreeChanged();
}

void AYChipPlugin::handleAsyncUpdate() {
}

void AYChipPlugin::initialise(const te::PluginInitialisationInfo&) {
    const juce::ScopedLock sl(lock);
    chip = std::make_unique<AyumiEmulator>(sampleRate);
    chip->setMasterVolume(0.1f);
}

void AYChipPlugin::deinitialise() {
}

void AYChipPlugin::reset() {
    const juce::ScopedLock sl(lock);
    if (chip) {
        chip->ResetSound();
    }
}

void AYChipPlugin::midiPanic() {
    reset();
}

void AYChipPlugin::applyToBuffer(const te::PluginRenderContext& fc) {
    if (!fc.isPlaying || fc.destBuffer == nullptr || fc.bufferForMidiMessages == nullptr) {
        return;
    }

    SCOPED_REALTIME_CHECK
    const juce::ScopedLock sl(lock);

    te::clearChannels(*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

    // Process PSG regiser events, no midi notes on this low level
    int currentSample = 0;
    PsgRegsAYFrame regs {};
    for (auto& m : *fc.bufferForMidiMessages) {
        // process up to this event
        const int timeSample = juce::roundToInt(m.getTimeStamp() * sampleRate);
        if (timeSample - currentSample > 0) {
            chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample), fc.destBuffer->getWritePointer(1, currentSample),
                               static_cast<size_t>(timeSample - currentSample));
            currentSample = timeSample;
        }
        if (m.isNoteOn()) {
            // note based PSG-MIDI mapping is deprecated
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
                regs.registers[reg] = static_cast<unsigned char>((val << 4) | regs.registers[reg]);
                regs.mask[reg] = !regs.mask[reg];
                // DBG("register coarse " << reg << ", " << m.getControllerValue() << ", mask " << (regs.mask[reg] ? "on" : "off"));
            } else if (40 <= ctrlNum && ctrlNum < 54) {
                // fine value
                reg = static_cast<size_t>(ctrlNum - 40);
                regs.registers[reg] = static_cast<unsigned char>(val | regs.registers[reg]);
                regs.mask[reg] = !regs.mask[reg];
                // DBG("register fine " << reg << ", " << m.getControllerValue() << ", mask " << (regs.mask[reg] ? "on" : "off") << " reg is " << regs.registers[reg]);
            } else {
                jassertfalse;
            }
            if (!regs.mask[reg]) {
                chip->setRegister(reg, regs.registers[reg]);
                // DBG("setRegister(" << reg << ", " << regs.registers[reg] << ")");
                regs.registers[reg] = 0;
            }
            // break;
        }
    }
    // process to the end of the block
    if (currentSample < fc.destBuffer->getNumSamples()) {
        chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample), fc.destBuffer->getWritePointer(1, currentSample),
                           static_cast<size_t>(fc.bufferNumSamples - currentSample));
    }
}

void AYChipPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v) {
    te::copyValueTree(state, v, getUndoManager());
}

//==============================================================================

} // namespace MoTool::uZX
