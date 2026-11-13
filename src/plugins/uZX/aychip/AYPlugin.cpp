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
    timerChannels.store(0, std::memory_order_relaxed);
    timerSync_ = {};
    timerSyncChannels_ = 0;
}

juce::String AYChipPlugin::describeTimerWarnings(uint32_t warnings) {
    juce::StringArray lines;
    if ((warnings & timerBankNotFound) != 0)
        lines.add("Timer effects are not playing: the lanes and actions they read were not found.");
    if ((warnings & timerCommandNoBank) != 0)
        lines.add("Timer effects are not playing: timers were started with nothing chosen for them to read.");
    if ((warnings & timerCommandIgnored) != 0)
        lines.add("Some timer effects are not playing: their commands name timers, lanes or actions that are not there.");
    return lines.joinIntoString("\n");
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
    timerWarnings.store(0, std::memory_order_relaxed);
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
    timers.setMuted(channelMuter.getMutedTimerRegisters(), *chip);

    const bool hasRegisters = ! frameDecoder.isEmpty();
    if (hasRegisters && staticParams.monitorMode.getStoredValue()) {
        frameDecoder.getFrame().debugPrintSet();
    }
    if (hasRegisters || ! timerDecoder.isEmpty()) {
        // The frame takes effect at the sample rendered up to: its timers start there, and the registers no
        // timer owns are written. A sample is some forty of the chip's cycles, and a timer can expire in this
        // one before the frame's cycle: that expiry comes first, as it does in the chip's time, so that it
        // takes its next interval from the values the frame is about to change
        const auto cycle = chipTime.cycleAt(samplesRendered);
        if (! timers.isIdle())
            writeTimersBefore(cycle);
        uint32_t warnings = timerWarnings.load(std::memory_order_relaxed);
        if (const auto bank = timerDecoder.takeBank()) {
            auto found = TimerBankRegistry::getInstance().find(*bank);
            // A bank that is found, or none asked for, ends what was wrong about the bank
            warnings &= ~uint32_t {timerBankNotFound | timerCommandNoBank};
            if (found == nullptr && *bank != 0)
                warnings |= timerBankNotFound;
            timers.setBank(std::move(found));
        }
        timers.applyFrame(timerDecoder.takeCommands(), hasRegisters ? frameDecoder.takeRegisters() : PsgRegsFrame {},
                          cycle, *chip);
        const auto faults = timers.takeFaults();
        // With a bank that was not found, its commands have none: that is the one thing wrong
        if ((faults & ChipTimers::commandWithoutBank) != 0 && (warnings & timerBankNotFound) == 0)
            warnings |= timerCommandNoBank;
        if ((faults & (ChipTimers::commandRefused | ChipTimers::tablesFailed)) != 0)
            warnings |= timerCommandIgnored;
        timerWarnings.store(warnings, std::memory_order_relaxed);
        timerChannels.store(findTimerChannels(timers.getOwned()), std::memory_order_relaxed);
        timerSyncChannels_ = findTimerChannels(static_cast<uint16_t>(timers.getOwned() & ~timers.getMuted()));
        for (auto& sync : timerSync_)
            sync.timer = -1;  // a timer that stopped no longer keeps a higher one from the channel's trigger
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
    writeTimerSync(lanes + kNumVizChannels, samples);
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
        resetTimers();  // play jumped or stopped: a timer starts again when its values are stated where play resumes
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
            writeTimerRegister(write);
        }
    }
    return position;
}

void AYChipPlugin::writeTimersBefore(uint64_t cycle) {
    const bool ym = staticParams.chipType.getStoredValue() == TypeEnum::YM;
    for (bool more = true; more;) {
        std::span<const TaymTimerWrite> writes;
        more = timers.collect(cycle, ym, writes);
        for (const auto& write : writes)
            writeTimerRegister(write);
    }
}

void AYChipPlugin::writeTimerSync(float* const* sync, size_t numSamples) noexcept {
    // Below 20 Hz a repeat is not a pitch
    const auto longestRepeat = static_cast<uint32_t>(sampleRate / 20.0);
    for (size_t chan = 0; chan < timerSync_.size(); ++chan) {
        auto& state = timerSync_[chan];
        const bool starts = std::exchange(state.due, false);
        if (starts) {
            state.pitched = state.sinceRepeat <= longestRepeat;
            state.sinceRepeat = 0;
        } else if (state.sinceRepeat > longestRepeat) {
            state.pitched = false;
        }
        state.sinceRepeat += static_cast<uint32_t>(jmin(numSamples, size_t {1} << 30));
        state.sinceRepeat = jmin(state.sinceRepeat, uint32_t {1} << 30);

        if (state.pitched && (timerSyncChannels_ & (1u << chan)) != 0) {
            std::fill_n(sync[chan], numSamples, 0.0f);
            if (starts)
                sync[chan][0] = 1.0f;
        }
    }
}

uint8_t AYChipPlugin::findTimerChannels(uint16_t owned) const noexcept {
    if (owned == 0)
        return 0;
    const auto has = [owned] (size_t reg) { return (owned & (1u << reg)) != 0; };
    const bool envelope = has(11) || has(12) || has(13);
    uint8_t channels = 0;
    for (size_t chan = 0; chan < 3; ++chan) {
        const bool usesEnvelope = (chip->getRegister(8 + chan) & 0x10) != 0;
        const bool noiseOn = ((chip->getRegister(7) >> (3 + chan)) & 1) == 0;
        if (has(8 + chan) || has(2 * chan) || has(2 * chan + 1) || has(7) || (envelope && usesEnvelope) || (has(6) && noiseOn))
            channels = static_cast<uint8_t>(channels | (1u << chan));
    }
    return channels;
}

void AYChipPlugin::writeTimerRegister(const TaymTimerWrite& write) noexcept {
    if (timers.isMuted(write.target))
        return;  // its timers are switched off: the register has what the frames give it
    chip->setRegister(write.target, static_cast<unsigned char>(write.value));
    // A mute holds over the registers as written: a write to the mixer or a volume would undo it
    if (write.target >= 7 && write.target <= 10)
        channelMuter.applyToChip(*chip);

    // The scopes of the channels this register reaches trigger where the lowest such timer's output repeats
    const auto reached = findTimerChannels(static_cast<uint16_t>(1u << write.target));
    for (size_t chan = 0; chan < timerSync_.size(); ++chan) {
        if ((reached & (1u << chan)) == 0)
            continue;
        auto& sync = timerSync_[chan];
        if (sync.timer < 0 || write.timer < sync.timer)
            sync.timer = static_cast<int8_t>(write.timer);
        if (sync.timer == write.timer && write.loop_start != 0)
            sync.due = true;
    }
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
