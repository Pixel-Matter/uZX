#include "AYPlugin.h"
#include "AYPluginEditor.h"


namespace MoTool::uZX {

//==============================================================================
const char* AYChipPlugin::xmlTypeName = "aychip";

AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : PluginBase(info)
    , midiParamsReader(staticParams.baseMidiChannel.getStoredValue())
{
    staticParams.referTo(state, getUndoManager());
    dynamicParams.referTo(state, getUndoManager());
    dynamicParams.visit([this](auto& vd) {
        addParam(vd);
    });
}

AYChipPlugin::~AYChipPlugin() {
    notifyListenersOfDeletion();
}

void AYChipPlugin::valueTreeChanged() {
    te::Plugin::valueTreeChanged();
}

void AYChipPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    // TODO staticParams.isParamProperty(id);
    if (v == state) {
        if (id == IDs::clock || id == IDs::chip) {
            DBG("id " << id << " changed to " << state.getProperty(id).toString());
            reset();
        } else if (id == IDs::volume) {
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                chip->setMasterVolume(dynamicParams.volume.getLiveValue());
            }
        } else if (id == IDs::stereo || id == IDs::layout) {
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                dynamicParams.layout.value.forceUpdateOfCachedValue();
                chip->setLayoutAndStereoWidth(dynamicParams.layout.getLiveValue(), dynamicParams.stereoWidth.getLiveValue());
            }
        } else if (id == IDs::midi) {
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                // it a static params, so it is ok to mute sounds
                chip->muteSound();
                staticParams.baseMidiChannel.value.forceUpdateOfCachedValue();
                midiParamsReader.setBaseChannel(staticParams.baseMidiChannel.getStoredValue());
            }
        }
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
                                               staticParams.chipType.getStoredValue());
    } else {
        chip->reset(static_cast<int>(sampleRate), staticParams.chipClock.getStoredValue() * MHz,
                                                  staticParams.chipType.getStoredValue());
    }
    chip->setMasterVolume(dynamicParams.volume.getLiveValue());
    chip->setLayoutAndStereoWidth(dynamicParams.layout.getLiveValue(), dynamicParams.stereoWidth.getLiveValue());
    // timeFromReset = 0.0;  // not used now
    midiParamsReader.reset();
    // midiRegsReader.reset();
    registersFrame = {};
}
void AYChipPlugin::updateRegistersFromMidiParams() noexcept {
    // DBG("updateRegistersFromMidiParams");
    auto& params = midiParamsReader.getParams();
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
    } else if (midiReaderMode == MidiReaderMode::Regs) {
        // updateRegistersFromMidiRegs();
    }

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
    } else if (midiReaderMode == MidiReaderMode::Regs) {
        // midiRegsReader.read(m);
    }
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

    te::clearChannels(*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);
    // Process PSG regiser events, no midi notes on this low level
    int currentSample = 0;
    // if (fc.bufferForMidiMessages->isNotEmpty()) {
        // DBG("AYChipPlugin: processing " << fc.bufferForMidiMessages->size() << " midi messages");
    // }
    for (auto& m : *fc.bufferForMidiMessages) {
        // TODO retrigger events with smallest delay

        const int timeSample = roundToInt(m.getTimeStamp() * sampleRate);
        if (timeSample > currentSample) {
            updateChip();
            chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample),
                               fc.destBuffer->getWritePointer(1, currentSample),
                               static_cast<size_t>(timeSample - currentSample),
                               staticParams.removeDC.getStoredValue());
            currentSample = timeSample;
        }
        // DBG("AY in midi " << m.getDescription());
        handleMidiEvent(m);
    }
    // process to the end of the block
    updateChip();
    if (currentSample < fc.destBuffer->getNumSamples()) {
        chip->processBlock(fc.destBuffer->getWritePointer(0, currentSample),
                           fc.destBuffer->getWritePointer(1, currentSample),
                           static_cast<size_t>(fc.bufferNumSamples - currentSample),
                           staticParams.removeDC.getStoredValue());
    }
    // timeFromReset += (double) fc.destBuffer->getNumSamples() / sampleRate;
    // DBG("timeFromReset = " << timeFromReset);
}

void AYChipPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v) {
    staticParams.restoreStateFromValueTree(v);
    dynamicParams.restoreStateFromValueTree(v);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

std::unique_ptr<te::Plugin::EditorComponent> AYChipPlugin::createEditor() {
    return std::make_unique<AYPluginEditor>(AYChipPlugin::Ptr(this));
}

//==============================================================================

} // namespace MoTool::uZX
