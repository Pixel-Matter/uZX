#include "AYPlugin.h"
#include "AYPluginEditor.h"

namespace te = tracktion;

namespace MoTool::uZX {


//==============================================================================
void AYChipPlugin::Params::initialise() {
    clockValue          .referTo(IDs::clock,  "Clock frequncy",    {0.894887, 2.0, 0.01},  1.7734, "MHz");
    chipTypeValue       .referTo(IDs::chip,   "Chip type",         ChipType::getLabels(),       ChipType::AY);
    channelsLayoutValue .referTo(IDs::layout, "Channels layout",   ChannelsLayout::getLabels(), ChannelsLayout::ACB);
    removeDCValue       .referTo(IDs::noDC,   "Remove DC",                                 true);
    baseMidiChannelValue.referTo(IDs::midi,   "Base MIDI channel", {1, 15 - 4, 1},         1);
    stereoWidthValue    .referTo(IDs::stereo, "Stereo width",      {0.0, 1.0, 0.01},       0.5);
}


void AYChipPlugin::Params::restoreFromTree(const juce::ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        chipTypeValue.cachedValue,
        clockValue.cachedValue,
        channelsLayoutValue.cachedValue,
        removeDCValue.cachedValue,
        baseMidiChannelValue.cachedValue,
        stereoWidthValue.cachedValue
    );
}

//==============================================================================
AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
    , staticParams(*this)
    , midiParamsReader(staticParams.baseMidiChannelValue)
{
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
    if (v == state) {
        if (id == IDs::clock || id == IDs::chip) {
            reset();
        } else if (id == IDs::stereo || id == IDs::layout) {
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                chip->setLayoutAndStereoWidth(staticParams.channelsLayoutValue, staticParams.stereoWidthValue);
            }
        } else if (id == IDs::midi) {
            const ScopedLock sl(lock);
            // it a static params, so it is ok to mute sounds
            chip->muteSound();
            midiParamsReader.setBaseChannel(staticParams.baseMidiChannelValue);
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
        chip = std::make_unique<AyumiEmulator>(sampleRate, staticParams.clockValue * MHz, staticParams.chipTypeValue);
    } else {
        chip->reset(static_cast<int>(sampleRate), staticParams.clockValue * MHz, staticParams.chipTypeValue);
    }
    chip->setMasterVolume(0.5f);
    chip->setLayoutAndStereoWidth(staticParams.channelsLayoutValue, staticParams.stereoWidthValue);
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
                               staticParams.removeDCValue);
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
                           staticParams.removeDCValue);
    }
    // timeFromReset += (double) fc.destBuffer->getNumSamples() / sampleRate;
    // DBG("timeFromReset = " << timeFromReset);
}

void AYChipPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v) {
    staticParams.restoreFromTree(v);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

std::unique_ptr<te::Plugin::EditorComponent> AYChipPlugin::createEditor() {
    return std::make_unique<AYPluginEditor>(AYChipPlugin::Ptr(this));
}

//==============================================================================

} // namespace MoTool::uZX
