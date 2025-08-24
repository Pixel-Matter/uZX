#include "Transport.h"

#include "../../controllers/MainCommands.h"
#include "../../models/Timecode.h"

#include <common/Utilities.h>


namespace MoTool {

using namespace Commands;

TransportBar::TransportBar(te::Edit& edit)
    : edit_{edit}
    , transport_{edit_.getTransport()}
{
    transport_.addChangeListener(this);
    transport_.state.addListener(this);
    timecodeFormat.referTo(edit.state, te::IDs::timecodeFormat, &edit_.getUndoManager());

    ::Helpers::addAndMakeVisible (*this, {
        &rewindButton_,
        &playPauseButton_,
        &recordButton_,
        &bpmLabel_,
        &timeSigLabel_,
        &transportReadout_,
        &masterVolumeSlider_
    });
    if (auto mgr = edit_.engine.getUIBehaviour().getApplicationCommandManager()) {
        rewindButton_.setCommandToTrigger(mgr, MainAppCommands::transportToStart, true);
        playPauseButton_.setCommandToTrigger(mgr, MainAppCommands::transportPlay, true);
    }
    recordButton_.onClick = [this] {
        bool wasRecording = edit_.getTransport().isRecording();
        if (!wasRecording) {
            edit_.engine.getUIBehaviour().getApplicationCommandManager()->invokeDirectly(MainAppCommands::transportRecord, false);
        } else {
            edit_.engine.getUIBehaviour().getApplicationCommandManager()->invokeDirectly(MainAppCommands::transportRecordStop, false);
        }
    };
    updatePlayButtonText(transport_.isPlaying());
    updateRecordButtonText(transport_.isRecording());
    updateTimeLabels(transport_.getPosition());
}

TransportBar::~TransportBar() {
    transport_.removeChangeListener(this);
    transport_.state.removeListener(this);
}

void TransportBar::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
}

void TransportBar::resized() {
    auto b = getLocalBounds();
    int w = 60;
    bpmLabel_.setBounds(b.removeFromLeft(w * 2).reduced(2));
    timeSigLabel_.setBounds(b.removeFromLeft(w * 2).reduced(2));
    transportReadout_.setBounds(b.removeFromLeft(w * 2).reduced(2));
    rewindButton_.setBounds(b.removeFromLeft(w).reduced(2));
    playPauseButton_.setBounds(b.removeFromLeft(w).reduced(2));
    recordButton_.setBounds(b.removeFromLeft(w).reduced(2));
    masterVolumeSlider_.setBounds(b.removeFromRight(w).expanded(4, 4));
}

void TransportBar::changeListenerCallback(ChangeBroadcaster*) {
    updatePlayButtonText(transport_.isPlaying());
    updateRecordButtonText(transport_.isRecording());
    updateTimeLabels(transport_.getPosition());
}

void TransportBar::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree == edit_.state && prop == te::IDs::timecodeFormat) {
        // timecodeFormat = TimecodeDisplayFormatExt::fromString(edit_.state[te::IDs::timecodeFormat].toString());
    } else if (tree == transport_.state && prop == te::IDs::position) {
        updateTimeLabels(transport_.getPosition());
    }
}

void TransportBar::updatePlayButtonText(bool isPlaying) {
    playPauseButton_.setButtonText(isPlaying ? "||" : "[>]");
}

void TransportBar::updateRecordButtonText(bool isRecording) {
    recordButton_.setButtonText(isRecording ? "Stop" : "Rec");
}

String TransportBar::getTimecode(te::TimePosition pos) const {
    return timecodeFormat->toFullTimecode(pos, 100, true);
    // return timecodeFormat->getString(edit_.tempoSequence, pos, true);
}

void TransportBar::updateTimeLabels(te::TimePosition pos) {
    static te::TimePosition lastPosition = te::TimePosition::fromSeconds(-1.0);
    if (lastPosition == pos) return;
    lastPosition = pos;
    auto& ts = edit_.tempoSequence;
    auto t = getTimecode(pos);
    transportReadout_.setText("Pos: " + t, dontSendNotification);
    bpmLabel_.setText(String::formatted("BPM: %.2f", ts.getBpmAt(pos)), dontSendNotification);
    timeSigLabel_.setText(String::formatted("Sig: " + ts.getTimeSigAt(pos).getStringTimeSig()), dontSendNotification);
}

} // namespace MoTool
