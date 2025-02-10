#pragma once

#include "Commands.h"

#include <JuceHeader.h>
#include <common/Utilities.h>


using namespace juce;

namespace MoTool {

using namespace Commands;

class TransportBar: public Component,
                    private ChangeListener {
public:

    explicit TransportBar(te::Edit& edit)
        : edit_ {edit}
        , transport_ {edit_.getTransport()}
    {
        transport_.addChangeListener(this);
        updatePlayButtonText();

        Helpers::addAndMakeVisible (*this, {
            &playPauseButton_,
            &recordButton_
        });
        playPauseButton_.onClick = [] {
            getGlobalCommandManager().invokeDirectly(AppCommands::transportPlay, false);
        };
        recordButton_.onClick = [this] {
            bool wasRecording = edit_.getTransport().isRecording();
            if (!wasRecording) {
                getGlobalCommandManager().invokeDirectly(AppCommands::transportRecord, false);
            } else {
                getGlobalCommandManager().invokeDirectly(AppCommands::transportRecordStop, false);
            }        };
    }

    void resized() override {
        // TODO use layout system
        auto b = getLocalBounds();
        int w = 60;
        int side = (b.getWidth() - w * 2) / 2;
        b.removeFromLeft(side);
        b.removeFromRight(side);
        playPauseButton_.setBounds(b.removeFromLeft(w).reduced(2));
        recordButton_.setBounds(b.removeFromLeft(w).reduced(2));
    }

    void changeListenerCallback (ChangeBroadcaster*) override {
        // if (source == &edit.getTransport()) { // not needed
        updatePlayButtonText();
        updateRecordButtonText();
        // }
    }

private:
    te::Edit& edit_;
    te::TransportControl& transport_;

    TextButton playPauseButton_ { "Play" },
               recordButton_    { "Rec" };

    void updatePlayButtonText() {
        playPauseButton_.setButtonText(transport_.isPlaying() ? "||" : "[>]");
    }

    void updateRecordButtonText() {
        recordButton_.setButtonText(edit_.getTransport().isRecording() ? "Stop" : "Rec");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

}  // namespace MoTool
