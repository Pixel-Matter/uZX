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
                // The timers count in the chip's clock: another clock stops them
                chipTime.clockHz = getClockHz();
                timers.setClock(getClockHz());
            }
        } else if (id == IDs::midi) {
            if (chip != nullptr) {
                const ScopedLock sl(lock);
                // it a static params, so it is ok to mute sounds
                chip->muteSound();
                resetTimers();
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
    if (chip != nullptr) {  // none until initialised and after deinitialise
        chip->muteSound();
        resetTimers();
    }
}

void AYChipPlugin::resetTimers() noexcept {
    timers.reset();
    timerDecoder.reset();
}

uint32_t AYChipPlugin::getClockHz() const {
    return static_cast<uint32_t>(roundToInt(staticParams.chipClock.getStoredValue() * MHz));
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

    chipTime = {getClockHz(), static_cast<uint64_t>(jmax(1.0, sampleRate))};
    samplesRendered = 0;
    timers.setClock(getClockHz());
    resetTimers();
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
    const bool hasRegisters = ! frameDecoder.isEmpty();
    if (hasRegisters && staticParams.monitorMode.getStoredValue()) {
        frameDecoder.getFrame().debugPrintSet();
    }
    if (hasRegisters || ! timerDecoder.isEmpty()) {
        // The frame takes effect at the sample rendered up to: its timers start there, and the registers no
        // timer owns are written
        if (const auto bank = timerDecoder.takeBank())
            timers.setBank(TimerBankRegistry::getInstance().find(*bank));
        timers.applyFrame(timerDecoder.takeCommands(), hasRegisters ? frameDecoder.takeRegisters() : PsgRegsFrame {},
                          chipTime.cycleAt(samplesRendered), *chip);
    }

    // Apply channel and effect filters directly to chip registers
    channelMuter.applyToChip(*chip);
}

// called under lock
void AYChipPlugin::handleMidiEvent(const te::MidiMessageWithSource& m) noexcept {
    if (! m.isController()) {
        return;
    }
    const int channel = m.getChannel() - staticParams.baseMidiChannel.getStoredValue();
    // The chip's four channels, then a channel for each timer; the timers' bank is on the fourth
    if (channel < PsgCC::numChannels
        && frameDecoder.decode(channel, m.getControllerNumber(), m.getControllerValue(), [this] { return readRegisters(); }))
        return;
    timerDecoder.decode(channel, m.getControllerNumber(), m.getControllerValue());
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
        resetTimers();  // play jumped or stopped: a timer is silent until its next start
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
    const int rendered = timers.isIdle() ? from : renderTimerWrites(fc, from, to);
    if (to > rendered)
        renderChannels(fc, rendered, to);
    samplesRendered += static_cast<uint64_t>(to - from);
    return to;
}

int AYChipPlugin::renderTimerWrites(const te::PluginRenderContext& fc, int from, int to) {
    // Sample `from` of the block is sample samplesRendered of chip time
    const auto endCycle = chipTime.firstCycleOf(samplesRendered + static_cast<uint64_t>(to - from));
    const bool ym = staticParams.chipType.getStoredValue() == TypeEnum::YM;
    int position = from;

    for (bool more = true; more;) {
        std::span<const TaymTimerWrite> writes;
        more = timers.collect(endCycle, ym, writes);
        for (const auto& write : writes) {
            const auto sample = static_cast<int64>(chipTime.sampleAt(write.cycle)) - static_cast<int64>(samplesRendered);
            const int at = from + static_cast<int>(jlimit<int64>(position - from, to - from, sample));
            if (at > position) {
                renderChannels(fc, position, at);
                position = at;
            }
            chip->setRegister(write.target, static_cast<unsigned char>(write.value));
            // A mute holds over the registers as written: a write to the mixer or a volume would undo it
            if (write.target >= 7 && write.target <= 10)
                channelMuter.applyToChip(*chip);
        }
    }
    return position;
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
