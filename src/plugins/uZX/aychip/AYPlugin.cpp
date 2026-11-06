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
    scopeSettings.referTo(state, getUndoManager());
    midiParamsReader.setBaseChannel(staticParams.baseMidiChannel.getStoredValue());
}

AYChipPlugin::~AYChipPlugin() {
    notifyListenersOfDeletion();
}

void AYChipPlugin::toggleSolo(size_t channel) {
    auto& solo = channelMuter.getSoloParam(channel);
    const bool soloing = ! solo.getStoredValue();
    solo.setStoredValue(soloing);
    if (soloing)  // a channel is on, muted or soloed: a muted one is soloed instead
        channelMuter.getChannelParam(channel).setStoredValue(true);

    // The track is soloed while any of its chip's channels is, so the other tracks go silent.
    if (auto* track = getOwnerTrack())
        track->setSolo(channelMuter.hasSolo());
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
        if (id == IDs::clock || id == IDs::chip) {
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

void AYChipPlugin::initialise(const te::PluginInitialisationInfo& info) {
    reset();
    const ScopedLock sl(lock);  // an old graph may still be rendering into this buffer
    rawChannelBuffer_.setSize(kNumVizChannels, info.blockSizeSamples);
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
        chip = std::make_unique<AyumiEmulator>(static_cast<int>(sampleRate), staticParams.chipClock.getStoredValue() * MHz,
                                               staticParams.chipType.getStoredValue());
    } else {
        chip->reset(static_cast<int>(sampleRate), staticParams.chipClock.getStoredValue() * MHz,
                    staticParams.chipType.getStoredValue());
    }
    updateDynamicParams();
    midiParamsReader.reset();
    registersFrame = {};
}

void AYChipPlugin::updateRegistersFromMidiParams() noexcept {
    // DBG("updateRegistersFromMidiParams");
    auto& params = midiParamsReader.getParams();
    if (staticParams.monitorMode.getStoredValue()) {
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

    // Set registers from the frame to the chip
    for (size_t i = 0; i < registersFrame.size(); ++i) {
        if (registersFrame.isSet(i)) {
            // DBG("" << i << " = " << registersFrame.getRaw(i));
            chip->setRegister(i, registersFrame.getRaw(i));
        }
    }

    // Apply channel and effect filters directly to chip registers
    channelMuter.applyToChip(*chip);
}

void AYChipPlugin::handleMidiEvent(const te::MidiMessageWithSource& m) noexcept {
    if (midiReaderMode == MidiReaderMode::Params) {
        midiParamsReader.read(m);
    }
    // else if (midiReaderMode == MidiReaderMode::Regs) {
    //     // midiRegsReader.read(m);
    // }
}

void AYChipPlugin::copyTempBufferToVizBuffer() {
    for (size_t chan = 0; chan < kNumVizChannels; ++chan) {
        auto* src = rawChannelBuffer_.getReadPointer(static_cast<int>(chan));
        vizBuffers_[chan].pushSamples(src, rawChannelBuffer_.getNumSamples());
    }
}

void AYChipPlugin::renderChannels(const te::PluginRenderContext& fc, int currentSample, int timeSample) {
    const auto samples = static_cast<size_t>(timeSample - currentSample);
    auto left = fc.destBuffer->getWritePointer(0, currentSample);
    auto right = fc.destBuffer->getWritePointer(1, currentSample);
    rawChannelBuffer_.setSize(kNumVizChannels, static_cast<int>(samples), false, false, true);
    chip->processBlockStereo(left, right, samples, staticParams.removeDC.getStoredValue(),
                             rawChannelBuffer_.getArrayOfWritePointers());
    copyTempBufferToVizBuffer();
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

    // Process PSG register events
    int currentSample = 0;
    auto lastTimestamp = std::numeric_limits<double>::lowest();
    for (auto& m : *fc.bufferForMidiMessages) {
        updateDynamicParams();

        // Apply the pending frame before reading a newer one, even within the same sample:
        // the reader drops unapplied values once the timestamp advances.
        if (m.getTimeStamp() > lastTimestamp) {
            updateChip();
            lastTimestamp = m.getTimeStamp();
        }

        // Upstream plugins can pass events stamped past the block end; never render beyond it.
        const int timeSample = jmin(fc.bufferNumSamples, roundToInt(m.getTimeStamp() * sampleRate));
        if (timeSample > currentSample) {
            renderChannels(fc, currentSample, timeSample);
            currentSample = timeSample;
        }
        // DBG("AY in midi " << m.getDescription());
        handleMidiEvent(m);
    }
    updateDynamicParams();
    // process to the end of the block
    updateChip();
    if (currentSample < fc.bufferNumSamples) {
        renderChannels(fc, currentSample, fc.bufferNumSamples);
    }
}

PsgRegsFrame AYChipPlugin::getChipRegisters() const {
    const ScopedLock sl(lock);
    PsgRegsFrame regs;
    if (chip != nullptr) {
        for (size_t i = 0; i < regs.size(); ++i) {
            regs.registers[i] = chip->getRegister(i);
            regs.mask[i] = true;
        }
    }
    return regs;
}

void AYChipPlugin::restorePluginStateFromValueTree(const ValueTree& v) {
    staticParams.restoreStateFromValueTree(v);
    dynamicParams.restoreStateFromValueTree(v);
    scopeSettings.restoreStateFromValueTree(v);

    // IMPOTANT! To restore automated parameters properly
    PluginBase::restorePluginStateFromValueTree(v);
}

std::unique_ptr<te::Plugin::EditorComponent> AYChipPlugin::createEditor() {
    return nullptr;
    // return std::make_unique<AYPluginEditor>(AYChipPlugin::Ptr(this));
}

//==============================================================================

} // namespace MoTool::uZX
