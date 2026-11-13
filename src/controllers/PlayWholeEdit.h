#pragma once

#include <JuceHeader.h>

namespace MoTool {

/** Playback covers the whole edit: it stops once the end has been heard, unless recording
    or looping. An empty loop range becomes the whole edit, and a loop range that is the whole
    edit follows its end as clips change; any other loop range stays where it was set.
    Rides on the transport's periodic position updates; create one per edit. */
class PlayWholeEdit : private juce::ValueTree::Listener,
                      private juce::AsyncUpdater
{
public:
    explicit PlayWholeEdit(tracktion::Edit& e) : edit_{e}, end_{getEnd(e)} {
        edit_.state.addListener(this);
        handleAsyncUpdate();
    }

    ~PlayWholeEdit() override {
        edit_.state.removeListener(this);
    }

    /** Moves the transport back to the start when it is parked at or past the end,
        so Play after an automatic stop replays the edit instead of stopping at once. */
    static void rewindIfAtEnd(tracktion::Edit& edit) {
        auto& transport = edit.getTransport();
        if (transport.getPosition() >= getEnd(edit))
            transport.setPosition({});
    }

private:
    tracktion::Edit& edit_;
    tracktion::TimePosition end_;  // the edit's end when the loop range was last checked

    static tracktion::TimePosition getEnd(tracktion::Edit& edit) {
        return tracktion::toPosition(edit.getLength());
    }

    // Async: the edit's cached length is invalidated by its own listener, which may run after this one.
    void handleAsyncUpdate() override {
        auto& transport = edit_.getTransport();
        const auto loop = transport.getLoopRange();
        const auto end = getEnd(edit_);
        // Within a microsecond: a saved edit reads its times back from text
        constexpr double tolerance = 1.0e-6;
        const bool wholeEdit = loop.getStart().inSeconds() < tolerance
                               && std::abs((loop.getEnd() - end_).inSeconds()) < tolerance;
        if (loop.getLength().inSeconds() < tolerance || wholeEdit)
            transport.setLoopRange({tracktion::TimePosition(), end});
        end_ = end;
    }

    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& child) override {
        if (tracktion::Clip::isClipState(child) || tracktion::TrackList::isTrack(child))
            triggerAsyncUpdate();
    }

    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree& child, int) override {
        valueTreeChildAdded(child, child);
    }

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) override {
        if (tracktion::Clip::isClipState(tree) && (prop == tracktion::IDs::start || prop == tracktion::IDs::length)) {
            triggerAsyncUpdate();
            return;
        }

        auto& transport = edit_.getTransport();
        if (tree != transport.state || prop != tracktion::IDs::position
            || !transport.isPlaying() || transport.isRecording() || transport.looping)
            return;

        const auto end = getEnd(edit_);
        const auto position = tracktion::TimePosition::fromSeconds(static_cast<double>(tree[tracktion::IDs::position]));
        auto* context = transport.getCurrentPlaybackContext();
        if (position < end || (context != nullptr && context->getAudibleTimelineTime() < end))
            return;

        transport.stop(false, false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayWholeEdit)
};

}  // namespace MoTool
