#pragma once

#include "Commands.h"
#include "tracktion_core/utilities/tracktion_Time.h"

#include <JuceHeader.h>
#include <common/Utilities.h>


using namespace juce;

namespace MoTool {

using namespace Commands;

class TransportBar: public Component,
                    private Timer,
                    private ChangeListener {
public:

    explicit TransportBar(te::Edit& edit)
        : edit_ {edit}
        , transport_ {edit_.getTransport()}
    {
        transport_.addChangeListener(this);

        Helpers::addAndMakeVisible (*this, {
            &rewindButton_,
            &stepLeftButton_,
            &playPauseButton_,
            &recordButton_,
            &stepRightButton_,
            &bpmLabel_,
            &timeSigLabel_,
            &positionLabel_
        });
        rewindButton_.onClick = [] {
            getGlobalCommandManager().invokeDirectly(AppCommands::transportRewind, false);
        };
        stepLeftButton_.onClick = [] {
            // getGlobalCommandManager().invokeDirectly(AppCommands::transportStepBack, false);
        };
        playPauseButton_.onClick = [] {
            getGlobalCommandManager().invokeDirectly(AppCommands::transportPlay, false);
        };
        recordButton_.onClick = [this] {
            bool wasRecording = edit_.getTransport().isRecording();
            if (!wasRecording) {
                getGlobalCommandManager().invokeDirectly(AppCommands::transportRecord, false);
            } else {
                getGlobalCommandManager().invokeDirectly(AppCommands::transportRecordStop, false);
            }
        };
        stepRightButton_.onClick = [] {
            // getGlobalCommandManager().invokeDirectly(AppCommands::transportStepForward, false);
        };
        updatePlayButtonText();
        updateTimeLabels();
        startTimerHz(30);
    }

    void resized() override {
        // TODO use layout system
        auto b = getLocalBounds();
        int w = 60;
        // int numButtons = 11;
        // int side = (b.getWidth() - w * numButtons) / 2;
        // b.removeFromLeft(side);
        // b.removeFromRight(side);

        bpmLabel_.setBounds(b.removeFromLeft(w * 2).reduced(2));
        timeSigLabel_.setBounds(b.removeFromLeft(w * 2).reduced(2));
        positionLabel_.setBounds(b.removeFromLeft(w * 2).reduced(2));

        rewindButton_.setBounds(b.removeFromLeft(w).reduced(2));
        stepLeftButton_.setBounds(b.removeFromLeft(w).reduced(2));
        playPauseButton_.setBounds(b.removeFromLeft(w).reduced(2));
        recordButton_.setBounds(b.removeFromLeft(w).reduced(2));
        stepRightButton_.setBounds(b.removeFromLeft(w).reduced(2));
    }

private:
    te::Edit& edit_;
    te::TransportControl& transport_;

    TextButton rewindButton_    { "|<<" },
               stepLeftButton_  { "<" },
               playPauseButton_ { "Play" },
               recordButton_    { "Rec" },
               stepRightButton_ { ">" };

    Label      bpmLabel_      { "BPM",      "BPM:" },
               timeSigLabel_  { "TimeSig",  "Sig:" },
               positionLabel_ { "Position", "Pos:" };
    te::TimePosition lastPosition_ {te::TimePosition::fromSeconds(-1.0)};

    void changeListenerCallback (ChangeBroadcaster*) override {
        // if (source == &edit.getTransport()) { // not needed
        updatePlayButtonText();
        updateRecordButtonText();
        updateTimeLabels();
        // }
    }

    void timerCallback() override {
        updateTimeLabels();
        repaint();
    }

    void updatePlayButtonText() {
        playPauseButton_.setButtonText(transport_.isPlaying() ? "||" : "[>]");
    }

    void updateRecordButtonText() {
        recordButton_.setButtonText(transport_.isRecording() ? "Stop" : "Rec");
    }

    void updateTimeLabels() {
        auto pos = transport_.getPosition();
        if (lastPosition_ == pos) return;

        lastPosition_ = pos;
        auto& ts = edit_.tempoSequence;
        auto barsNbeats = ts.toBarsAndBeats(pos);
        auto detailed = String::formatted("Pos: %d.%.3f",
                                          barsNbeats.bars + 1,
                                          barsNbeats.beats.inBeats() + 1.0f);
        positionLabel_.setText(detailed, dontSendNotification);
        bpmLabel_.setText(String::formatted("BPM: %.2f", ts.getBpmAt(pos)), dontSendNotification);
        timeSigLabel_.setText(String::formatted("Sig: " + ts.getTimeSigAt(pos).getStringTimeSig()), dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

}  // namespace MoTool
