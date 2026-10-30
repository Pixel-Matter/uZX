#pragma once

#include <JuceHeader.h>

namespace MoTool {

/** Playback covers the whole edit: it stops once the end has been heard, unless recording
    or looping, and the loop range follows the edit from its start to its end.
    Rides on the transport's periodic position updates; create one per edit. */
class PlayWholeEdit : private juce::ValueTree::Listener,
                      private juce::AsyncUpdater
{
public:
    explicit PlayWholeEdit(tracktion::Edit& e) : edit_{e} {
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

    static tracktion::TimePosition getEnd(tracktion::Edit& edit) {
        return tracktion::toPosition(edit.getLength());
    }

    // Async: the edit's cached length is invalidated by its own listener, which may run after this one.
    // ponytail: overwrites any loop range; keep a user-set one once the ruler can set it (v1.0 Loop range)
    void handleAsyncUpdate() override {
        edit_.getTransport().setLoopRange({tracktion::TimePosition(), getEnd(edit_)});
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
