
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

    // edit.getTransport().addChangeListener(this);
}

void AYChipPlugin::deinitialise() {
    // edit.getTransport().removeChangeListener(this);
}

void AYChipPlugin::reset() {
    const juce::ScopedLock sl(lock);
    if (chip) {
        chip->ResetSound();
    }
}

// void AYChipPlugin::changeListenerCallback(ChangeBroadcaster* source) {
//     if (auto* transport = dynamic_cast<te::TransportControl*>(source); transport != nullptr && chip != nullptr) {
//         const ScopedLock sl(lock);
//         mutedWhileNotPlaying = !transport->isPlaying();
//     }}

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
    for (auto& m : *fc.bufferForMidiMessages) {
        if (m.isNoteOn()) {
            const int note = m.getNoteNumber();
            const size_t reg = static_cast<size_t>(note - 60);
            const unsigned char val = m.getVelocity();
            const int noteTimeSample = juce::roundToInt(m.getTimeStamp() * sampleRate);

            // process up to this event
            if (noteTimeSample - currentSample > 0) {
                chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample), fc.destBuffer->getWritePointer(1, currentSample),
                                   static_cast<size_t>(noteTimeSample - currentSample));
            }
            chip->setRegister(reg, val);
            // DBG("setRegister(" << reg << ", " << val << ")");
            currentSample = noteTimeSample;
        }
    }
    // process to the end of the block
    chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample), fc.destBuffer->getWritePointer(1, currentSample),
                       static_cast<size_t>(fc.bufferNumSamples - currentSample));
}

void AYChipPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v) {
    te::copyValueTree(state, v, getUndoManager());
}

//==============================================================================

} // namespace MoTool::uZX
