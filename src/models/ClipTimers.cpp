#include "ClipTimers.h"

#include "Ids.h"
#include "PsgClip.h"
#include "PsgList.h"
#include "TimerCC.h"

#include <algorithm>

namespace MoTool {

namespace {

// A timer lane reference in the state: an index, or one of TAYM's two markers as a small negative number
constexpr int laneNone = -1;
constexpr int laneUnchanged = -2;

int laneToState(uint32_t ref) noexcept {
    return ref == TAYM_TLAN_NONE ? laneNone : ref == TAYM_TLAN_UNCHANGED ? laneUnchanged : static_cast<int>(ref);
}

uint32_t laneFromState(int lane) noexcept {
    return lane == laneNone ? TAYM_TLAN_NONE : lane == laneUnchanged ? TAYM_TLAN_UNCHANGED : static_cast<uint32_t>(lane);
}

bool hasValues(const TaymMods& command) noexcept {
    return command.command == TAYM_CMD_START || command.command == TAYM_CMD_MODULATE;
}

}  // namespace

//==============================================================================
void ClipTimers::set(const juce::MemoryBlock& bank, int chipIndex, const std::vector<Event>& events, juce::UndoManager* um) {
    // Built detached, then attached in one step: one undo, and one change for whoever listens
    juce::ValueTree timers {IDs::TIMERS};
    timers.setProperty(IDs::bank, bank, nullptr);
    timers.setProperty(IDs::chip, chipIndex, nullptr);
    for (const auto& event : events) {
        juce::ValueTree v {IDs::TIMER};
        v.setProperty(IDs::i, event.frameIndex, nullptr);
        v.setProperty(IDs::timer, event.timer, nullptr);
        v.setProperty(IDs::cmd, static_cast<int>(event.command.command), nullptr);
        if (hasValues(event.command)) {
            v.setProperty(IDs::base, static_cast<juce::int64>(event.command.base_timer_value), nullptr);
            v.setProperty(IDs::lane, laneToState(event.command.timer_lane_ref), nullptr);
            v.setProperty(IDs::first, static_cast<juce::int64>(event.command.first_action), nullptr);
            v.setProperty(IDs::count, static_cast<int>(event.command.action_count), nullptr);
        }
        timers.appendChild(v, nullptr);
    }

    clipState_.removeChild(clipState_.getChildWithName(IDs::TIMERS), um);
    if (! events.empty())
        clipState_.addChild(timers, -1, um);
    reload();
}

void ClipTimers::reload() {
    events_.clear();
    registration_.reset();
    bank_.reset();

    const auto timers = clipState_.getChildWithName(IDs::TIMERS);
    if (! timers.isValid())
        return;

    if (const auto* bytes = timers[IDs::bank].getBinaryData())
        bank_ = TimerBank::fromTaym(bytes->getData(), bytes->getSize(),
                                    static_cast<size_t>(juce::jmax(0, static_cast<int>(timers[IDs::chip]))));
    if (bank_ == nullptr)
        return;  // commands without their bank index nothing
    registration_ = TimerBankRegistry::getInstance().add(bank_);

    events_.reserve(static_cast<size_t>(timers.getNumChildren()));
    for (const auto& v : timers) {
        if (! v.hasType(IDs::TIMER))
            continue;
        Event event;
        event.frameIndex = v[IDs::i];
        event.timer = v[IDs::timer];
        event.command.command = static_cast<uint8_t>(static_cast<int>(v[IDs::cmd]));
        if (hasValues(event.command)) {
            event.command.base_timer_value = static_cast<uint32_t>(static_cast<juce::int64>(v[IDs::base]));
            event.command.timer_lane_ref = laneFromState(v[IDs::lane]);
            event.command.first_action = static_cast<uint32_t>(static_cast<juce::int64>(v[IDs::first]));
            event.command.action_count = static_cast<uint8_t>(static_cast<int>(v[IDs::count]));
        }
        events_.push_back(event);
    }
    std::stable_sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) { return a.frameIndex < b.frameIndex; });
}

void ClipTimers::addToPlaybackSequence(juce::MidiMessageSequence& sequence, const PsgClip& clip,
                                       tracktion::MidiList::TimeBase timeBase) const {
    const int bankNumber = getBankNumber();
    if (events_.empty() || bankNumber == 0)
        return;

    const int channel = clip.getPsg().getMidiChannel().getChannelNumber();
    juce::MidiMessageSequence timers;
    const auto add = [&](double time, const TimerCC::Messages& messages) {
        for (const auto& m : messages)
            timers.addEvent(juce::MidiMessage::controllerEvent(channel + m.channel, static_cast<int>(m.controller), m.value), time);
    };

    // The bank where the sequence begins: the clip's start, or in raw beats the first frame, from where
    // Tracktion chases the controllers to the clip's start
    add(timeBase == tracktion::MidiList::TimeBase::beatsRaw ? PsgList::getFrameTimeInBase(0, clip, timeBase) : 0.0,
        TimerCC::encodeBank(bankNumber));

    TimerEncoder encoder;
    for (const auto& event : events_) {
        if (PsgList::isFrameBeforeClip(event.frameIndex, clip, timeBase))
            continue;
        add(PsgList::getFrameTimeInBase(event.frameIndex, clip, timeBase), encoder.encode(event.timer, event.command));
    }

    // Appended in time order above; merged with the frames' messages in one sort, which keeps equal times
    // in their order
    sequence.addSequence(timers, 0.0);
}

}  // namespace MoTool
