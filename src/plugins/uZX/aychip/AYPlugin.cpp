#include "AYPlugin.h"
// #include "AYPluginEditor.h"


namespace MoTool::uZX {

//==============================================================================
const char* AYChipPlugin::xmlTypeName = "aychip";

AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : PluginBase(info)
    , midiParamsReader(1)
{
    staticParams.referTo(state, getUndoManager());
    dynamicParams.referTo(state, getUndoManager());
    dynamicParams.visit([this](auto& vd) {
        addParam(vd);
    });
    channelMuter.referTo(state, getUndoManager());
    channelMuter.visit([this](auto& vd) {
        addParam(vd);
    });
    channelMuter.setupLinkedToggleBehavior();
    midiParamsReader.setBaseChannel(staticParams.baseMidiChannel.getStoredValue());
}

AYChipPlugin::~AYChipPlugin() {
    notifyListenersOfDeletion();
}

void AYChipPlugin::valueTreeChanged() {
    te::Plugin::valueTreeChanged();
}

void AYChipPlugin::updateDynamicParams() {
    chip->setMasterVolume(dynamicParams.volume.getLiveValue());
    chip->setLayoutAndStereoWidth(dynamicParams.layout.getLiveValue(), dynamicParams.stereoWidth.getLiveValue());
}

void AYChipPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    // TODO staticParams.isParamProperty(id);
    if (v == state) {
        if (id == IDs::clock || id == IDs::chip || id == IDs::numChannels) {
            reset();
        } else if (id == IDs::midi) {
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                // it a static params, so it is ok to mute sounds
                chip->muteSound();
                staticParams.baseMidiChannel.forceUpdateOfCachedValue();
                midiParamsReader.setBaseChannel(staticParams.baseMidiChannel.getStoredValue());
            }
        }
        // Commented out because updateFromDynamicParams is called in applyToBuffer
        // else if (id == IDs::volume) {
        //     if (chip != nullptr) {
        //         const ScopedLock sl(lock);
        //         chip->setMasterVolume(dynamicParams.volume.getLiveValue());
        //     }
        // } else if (id == IDs::stereo || id == IDs::layout) {
        //     if (chip != nullptr) {
        //         const ScopedLock sl(lock);
        //         dynamicParams.layout.value.forceUpdateOfCachedValue();
        //         chip->setLayoutAndStereoWidth(dynamicParams.layout.getLiveValue(), dynamicParams.stereoWidth.getLiveValue());
        //     }
        // }

        // no need to do anything
        // else if (id == IDs::noDC) {
        //     // DBG("removeDC = " << (staticParams.removeDCValue ? "true" : "false"));
        // }
        propertiesChanged();
    }
    Plugin::valueTreePropertyChanged(v, id);
}

void AYChipPlugin::initialise(const te::PluginInitialisationInfo&) {
    reset();
}

void AYChipPlugin::deinitialise() {
    const ScopedLock sl(lock);
    chip.reset();
}

void AYChipPlugin::midiPanic() {
    const ScopedLock sl(lock);
    chip->muteSound();
}

void AYChipPlugin::reset() {
    const ScopedLock sl(lock);
    if (chip == nullptr) {
        chip = std::make_unique<AyumiEmulator>(sampleRate, staticParams.chipClock.getStoredValue() * MHz,
                                               staticParams.chipType.getStoredValue(),
                                               staticParams.numOutputChannels.getStoredValue());
    } else {
        chip->reset(static_cast<int>(sampleRate), staticParams.chipClock.getStoredValue() * MHz,
                                                  staticParams.chipType.getStoredValue());
        chip->setOutputMode(staticParams.numOutputChannels.getStoredValue());
    }
    updateDynamicParams();
    // timeFromReset = 0.0;  // not used now
    midiParamsReader.reset();
    // midiRegsReader.reset();
    registersFrame = {};
}

void AYChipPlugin::updateRegistersFromMidiParams() noexcept {
    // DBG("updateRegistersFromMidiParams");
    auto& params = midiParamsReader.getParams();
    if (dynamicParams.monitorMode.getLiveValue()) {
        params.debugPrintSet();
    }
    registersFrame.clear();
    params.updateRegisters(registersFrame);
    params.clear();  // clear params after update
}

void AYChipPlugin::updateRegistersFromMidiRegs() noexcept {
    // DBG("updateRegistersFromMidiRegs");
    // registersFrame = midiRegsReader.getRegisters();
    // midiRegsReader.clear();
}

// called under lock
void AYChipPlugin::updateChip() noexcept {
    if (midiReaderMode == MidiReaderMode::Params) {
        updateRegistersFromMidiParams();
    }
    // else if (midiReaderMode == MidiReaderMode::Regs) {
    //     // updateRegistersFromMidiRegs();
    // }

    // Apply channel and effect filters
    channelMuter.apply(registersFrame);

    for (size_t i = 0; i < registersFrame.size(); ++i) {
        if (registersFrame.isSet(i)) {
            // DBG("" << i << " = " << registersFrame.getRaw(i));
            chip->setRegister(i, registersFrame.getRaw(i));
        }
    }
}

void AYChipPlugin::handleMidiEvent(const te::MidiMessageWithSource& m) noexcept {
    if (midiReaderMode == MidiReaderMode::Params) {
        midiParamsReader.read(m);
    }
    // else if (midiReaderMode == MidiReaderMode::Regs) {
    //     // midiRegsReader.read(m);
    // }
}

void AYChipPlugin::applyToBuffer(const te::PluginRenderContext& fc) noexcept {
    if (chip == nullptr || fc.destBuffer == nullptr || fc.bufferForMidiMessages == nullptr) {
        return;
    }

    SCOPED_REALTIME_CHECK
    const ScopedLock sl(lock);

    if (fc.bufferForMidiMessages->isAllNotesOff) {
        // DBG("AYChipPlugin: all notes off");
        chip->muteSound();
        // do not return here!
    }

    const int numChannels = staticParams.numOutputChannels.getStoredValue();
    const int actualChannels = fc.destBuffer->getNumChannels();

    // Safety check: if buffer doesn't have expected channels yet (during graph rebuild), use what's available
    const int channelsToUse = jmin(numChannels, actualChannels);

    if (channelsToUse < numChannels) {
        // Buffer doesn't have enough channels yet - audio graph is being rebuilt
        // Clear available channels and return early
        te::clearChannels(*fc.destBuffer, actualChannels, -1, fc.bufferStartSample, fc.bufferNumSamples);
        return;
    }

    te::clearChannels(*fc.destBuffer, numChannels, -1, fc.bufferStartSample, fc.bufferNumSamples);

    // Process PSG regiser events, no midi notes on this low level
    int currentSample = 0;
    // if (fc.bufferForMidiMessages->isNotEmpty()) {
        // DBG("AYChipPlugin: processing " << fc.bufferForMidiMessages->size() << " midi messages");
    // }
    for (auto& m : *fc.bufferForMidiMessages) {
        updateDynamicParams();
        // TODO retrigger events with smallest delay

        const int timeSample = roundToInt(m.getTimeStamp() * sampleRate);
        if (timeSample > currentSample) {
            updateChip();
            if (numChannels == 1) {
                chip->processBlockMono(fc.destBuffer->getWritePointer(0, currentSample),
                                       static_cast<size_t>(timeSample - currentSample),
                                       staticParams.removeDC.getStoredValue());
            } else if (numChannels == 2) {
                chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample),
                                   fc.destBuffer->getWritePointer(1, currentSample),
                                   static_cast<size_t>(timeSample - currentSample),
                                   staticParams.removeDC.getStoredValue());
            } else { // numChannels == 3
                chip->processBlockUnmixed(fc.destBuffer->getWritePointer(0, currentSample),
                                          fc.destBuffer->getWritePointer(1, currentSample),
                                          fc.destBuffer->getWritePointer(2, currentSample),
                                          static_cast<size_t>(timeSample - currentSample));
            }
            currentSample = timeSample;
        }
        // DBG("AY in midi " << m.getDescription());
        handleMidiEvent(m);
    }
    updateDynamicParams();
    // process to the end of the block
    updateChip();
    if (currentSample < fc.destBuffer->getNumSamples()) {
        if (numChannels == 1) {
            chip->processBlockMono(fc.destBuffer->getWritePointer(0, currentSample),
                                   static_cast<size_t>(fc.bufferNumSamples - currentSample),
                                   staticParams.removeDC.getStoredValue());
        } else if (numChannels == 2) {
            chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample),
                               fc.destBuffer->getWritePointer(1, currentSample),
                               static_cast<size_t>(fc.bufferNumSamples - currentSample),
                               staticParams.removeDC.getStoredValue());
        } else { // numChannels == 3
            chip->processBlockUnmixed(fc.destBuffer->getWritePointer(0, currentSample),
                                      fc.destBuffer->getWritePointer(1, currentSample),
                                      fc.destBuffer->getWritePointer(2, currentSample),
                                      static_cast<size_t>(fc.bufferNumSamples - currentSample));
        }
    }
    // timeFromReset += (double) fc.destBuffer->getNumSamples() / sampleRate;
    // DBG("timeFromReset = " << timeFromReset);
}

void AYChipPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v) {
    staticParams.restoreStateFromValueTree(v);
    dynamicParams.restoreStateFromValueTree(v);

    // IMPOTANT! To restore automated parameters properly
    PluginBase::restorePluginStateFromValueTree(v);
}

std::unique_ptr<te::Plugin::EditorComponent> AYChipPlugin::createEditor() {
    return nullptr;
    // return std::make_unique<AYPluginEditor>(AYChipPlugin::Ptr(this));
}

//==============================================================================

} // namespace MoTool::uZX
