#include "AYPlugin.h"
// #include "AYPluginEditor.h"


namespace MoTool::uZX {

//==============================================================================
const char* AYChipPlugin::xmlTypeName = "aychip";

AYChipPlugin::AYChipPlugin(te::PluginCreationInfo info)
    : PluginBase(info)
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
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                // A setting, not a stop: the chip keeps its registers and the frame being read
                staticParams.chipClock.forceUpdateOfCachedValue();
                staticParams.chipType.forceUpdateOfCachedValue();
                chip->setClock(staticParams.chipClock.getStoredValue() * MHz);
                chip->setType(staticParams.chipType.getStoredValue());
            }
        } else if (id == IDs::midi) {
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                // it a static params, so it is ok to mute sounds
                chip->muteSound();
                staticParams.baseMidiChannel.forceUpdateOfCachedValue();
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
    scopeLanes_.setSize(3 * kNumVizChannels, info.blockSizeSamples);
}

void AYChipPlugin::deinitialise() {
    const ScopedLock sl(lock);
    chip.reset();
}

void AYChipPlugin::midiPanic() {
    const ScopedLock sl(lock);
    if (chip != nullptr)  // none until initialised and after deinitialise
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
    frameDecoder = {};
}

// called under lock
PsgRegsFrame AYChipPlugin::readRegisters() const noexcept {
    PsgRegsFrame regs;
    for (size_t i = 0; i < regs.size(); ++i) {
        regs.registers[i] = chip->getRegister(i);
        regs.mask[i] = true;
    }
    return regs;
}

// called under lock
void AYChipPlugin::updateChip() noexcept {
    if (! frameDecoder.isEmpty()) {
        if (staticParams.monitorMode.getStoredValue()) {
            frameDecoder.getFrame().debugPrintSet();
        }
        const auto regs = frameDecoder.takeRegisters();
        for (size_t i = 0; i < regs.size(); ++i) {
            if (regs.isSet(i)) {
                chip->setRegister(i, regs.getRaw(i));
            }
        }
    }

    // Apply channel and effect filters directly to chip registers
    channelMuter.applyToChip(*chip);
}

// called under lock
void AYChipPlugin::handleMidiEvent(const te::MidiMessageWithSource& m) noexcept {
    if (! m.isController()) {
        return;
    }
    frameDecoder.decode(m.getChannel() - staticParams.baseMidiChannel.getStoredValue(), m.getControllerNumber(),
                        m.getControllerValue(), [this] { return readRegisters(); });
}

void AYChipPlugin::copyTempBufferToVizBuffer() {
    for (size_t chan = 0; chan < kNumVizChannels; ++chan) {
        const auto c = static_cast<int>(chan);
        vizBuffers_[chan].pushSamples(scopeLanes_.getReadPointer(c), scopeLanes_.getNumSamples(),
                                      scopeLanes_.getReadPointer(kNumVizChannels + c),
                                      scopeLanes_.getReadPointer(2 * kNumVizChannels + c));
    }
}

void AYChipPlugin::renderChannels(const te::PluginRenderContext& fc, int currentSample, int timeSample) {
    const auto samples = static_cast<size_t>(timeSample - currentSample);
    auto left = fc.destBuffer->getWritePointer(0, currentSample);
    auto right = fc.destBuffer->getWritePointer(1, currentSample);
    scopeLanes_.setSize(3 * kNumVizChannels, static_cast<int>(samples), false, false, true);
    auto* const* lanes = scopeLanes_.getArrayOfWritePointers();
    chip->processBlockStereo(left, right, samples, staticParams.removeDC.getStoredValue(),
                             {lanes, lanes + kNumVizChannels, lanes + 2 * kNumVizChannels});
    if (fc.isPlaying)  // stopped, the scopes keep the last frame that played
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

    // A frame is the messages of one time. It reaches the chip when a message of a later time arrives, after
    // the samples before that time are rendered, or at the end of the block
    int position = 0;
    auto frameTime = std::numeric_limits<double>::lowest();
    for (auto& m : *fc.bufferForMidiMessages) {
        updateDynamicParams();

        if (m.getTimeStamp() > frameTime) {
            // Apply the frame read so far before reading a newer one, even within the same sample
            updateChip();
            frameTime = m.getTimeStamp();
            // Upstream plugins can pass events stamped past the block end; never render beyond it.
            position = renderTo(fc, position, jmin(fc.bufferNumSamples, roundToInt(frameTime * sampleRate)));
        }
        // DBG("AY in midi " << m.getDescription());
        handleMidiEvent(m);
    }
    updateDynamicParams();
    updateChip();
    renderTo(fc, position, fc.bufferNumSamples);
}

int AYChipPlugin::renderTo(const te::PluginRenderContext& fc, int from, int to) {
    if (to <= from)
        return from;
    renderChannels(fc, from, to);
    return to;
}

PsgRegsFrame AYChipPlugin::getChipRegisters() const {
    const ScopedLock sl(lock);
    return chip != nullptr ? readRegisters() : PsgRegsFrame {};
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
