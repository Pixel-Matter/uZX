#pragma once

#include <JuceHeader.h>

namespace MoTool {

/** Stops playback once the end of the edit has been heard, unless recording or looping.
    Rides on the transport's periodic position updates; create one per edit. */
class PlayWholeEdit : private juce::ValueTree::Listener
{
public:
    explicit PlayWholeEdit(tracktion::Edit& e) : edit_{e} {
        edit_.getTransport().state.addListener(this);
    }

    ~PlayWholeEdit() override {
        edit_.getTransport().state.removeListener(this);
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

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) override {
        auto& transport = edit_.getTransport();
        if (tree != transport.state || prop != tracktion::IDs::position
            || !transport.isPlaying() || transport.isRecording() || transport.looping)
            return;

        // The transport's cached position may lag this notification, so read the tree.
        // It also runs ahead of the speakers by the output latency: wait until the end is audible.
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
