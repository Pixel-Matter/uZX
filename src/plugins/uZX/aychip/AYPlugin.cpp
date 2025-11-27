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

int AYChipPlugin::getNumOutputChannelsGivenInputs(int) {
    // Read directly from ValueTree state instead of CachedValue to get saved value during early initialization
    const int defaultChannels = staticParams.numOutputChannels.definition.defaultValue;
    const int numChannels = state.getProperty(IDs::numChannels, defaultChannels);

    // Channel mapping:
    // 1 (mono) → 2 channels (L, R)
    // 2 (stereo) → 2 channels (L, R)
    // 3 (separate) → 5 channels (L, R, A, B, C)
    const int result = (numChannels > 2) ? 5 : 2;
    DBG("AYChipPlugin::getNumOutputChannelsGivenInputs: numChannels=" << numChannels << " -> returning " << result);
    return result;
}

void AYChipPlugin::getChannelNames(StringArray*, StringArray* outs) {
    if (outs != nullptr) {
        outs->clear();
        getLeftRightChannelNames(outs);
        // Read directly from ValueTree state instead of CachedValue to get saved value during early initialization
        const int defaultChannels = staticParams.numOutputChannels.definition.defaultValue;
        const int numChannels = state.getProperty(IDs::numChannels, defaultChannels);
        DBG("AYChipPlugin::getChannelNames: numChannels from state=" << numChannels << " (default=" << defaultChannels << ")");
        if (numChannels > 2) {
            // Stereo mode outputs 5 channels: L, R, A, B, C
            outs->add("Channel A");
            outs->add("Channel B");
            outs->add("Channel C");
        }
        // output channel names to console
        DBG("AYChipPlugin::getChannelNames: output channels:");
        for (int i = 0; i < outs->size(); ++i) {
            DBG("  " << i << ": " << outs->getReference(i));
        }
    }
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
    // Read directly from ValueTree state instead of CachedValue to get saved value during early initialization
    const int defaultChannels = staticParams.numOutputChannels.definition.defaultValue;
    const int numChannels = state.getProperty(IDs::numChannels, defaultChannels);
    // Ayumi mode: 1=MONO, 3=SEPARATE (for stereo+separate output via processBlockStereoPlusSeparate)
    const int ayumiMode = (numChannels >= 2) ? 3 : 1;

    DBG("AYChipPlugin::reset: numChannels=" << numChannels << " ayumiMode=" << ayumiMode);

    if (chip == nullptr) {
        chip = std::make_unique<AyumiEmulator>(sampleRate, staticParams.chipClock.getStoredValue() * MHz,
                                               staticParams.chipType.getStoredValue(),
                                               ayumiMode);
    } else {
        chip->reset(static_cast<int>(sampleRate), staticParams.chipClock.getStoredValue() * MHz,
                                                  staticParams.chipType.getStoredValue());
        chip->setOutputMode(ayumiMode);
    }
    updateDynamicParams();
    midiParamsReader.reset();
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

void AYChipPlugin::renderChannels(const te::PluginRenderContext& fc, int currentSample, int timeSample) {
    const int numChannels = staticParams.numOutputChannels.getStoredValue();
    const int actualChannels = fc.destBuffer->getNumChannels();
    const auto samples = static_cast<size_t>(timeSample - currentSample);

    DBG("renderChannels: numChannels=" << numChannels << " actualChannels=" << actualChannels << " samples=" << samples);

    if (numChannels <=2 && actualChannels >= 2) {
        // Mono and stereo mode
        DBG("  -> processBlockStereo (actualChannels=" << actualChannels << ")");
        chip->processBlockStereo(fc.destBuffer->getWritePointer(0, currentSample),
                                 fc.destBuffer->getWritePointer(1, currentSample),
                                 samples,
                                 staticParams.removeDC.getStoredValue());
    } else if (numChannels > 2 && actualChannels >= 5) {
        // Stereo mode: Output 5 channels (L, R, A, B, C)
        DBG("  -> processBlockStereoPlusSeparate (5 channels)");
        chip->processBlockStereoPlusSeparate(
            fc.destBuffer->getWritePointer(0, currentSample),  // Left
            fc.destBuffer->getWritePointer(1, currentSample),  // Right
            fc.destBuffer->getWritePointer(2, currentSample),  // Channel A
            fc.destBuffer->getWritePointer(3, currentSample),  // Channel B
            fc.destBuffer->getWritePointer(4, currentSample),  // Channel C
            samples,
            staticParams.removeDC.getStoredValue());
    } else {
        DBG("  -> NO RENDER PATH MATCHED!" << " numChannels=" << numChannels << " actualChannels=" << actualChannels);
    }
}

void AYChipPlugin::applyToBuffer(const te::PluginRenderContext& fc) noexcept {
    if (chip == nullptr || fc.destBuffer == nullptr || fc.bufferForMidiMessages == nullptr) {
        return;
    }

    SCOPED_REALTIME_CHECK
    const ScopedLock sl(lock);

    if (fc.bufferForMidiMessages->isAllNotesOff) {
        chip->muteSound();
    }

    const int numChannels = staticParams.numOutputChannels.getStoredValue();
    // Actual output channels: 1 for mono, 5 for stereo (L, R, A, B, C)
    const int actualChannels = fc.destBuffer->getNumChannels();

    static int debugCounter = 0;
    if (debugCounter++ % 1000 == 0) {
        DBG("applyToBuffer: numChannels=" << numChannels << " actualChannels=" << actualChannels);
    }

    // Process PSG register events
    int currentSample = 0;
    for (auto& m : *fc.bufferForMidiMessages) {
        updateDynamicParams();

        const int timeSample = roundToInt(m.getTimeStamp() * sampleRate);
        if (timeSample > currentSample) {
            updateChip();
            renderChannels(fc, currentSample, timeSample);
            currentSample = timeSample;
        }
        // DBG("AY in midi " << m.getDescription());
        handleMidiEvent(m);
    }
    updateDynamicParams();
    // process to the end of the block
    updateChip();
    if (currentSample < fc.destBuffer->getNumSamples()) {
        renderChannels(fc, currentSample, fc.bufferNumSamples);
    }
}

void AYChipPlugin::restorePluginStateFromValueTree(const ValueTree& v) {
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
