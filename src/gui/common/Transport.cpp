#include "Transport.h"

#include "../../controllers/MainCommands.h"
#include "../../models/Timecode.h"
#include "juce_gui_basics/juce_gui_basics.h"

#include <common/Utilities.h>


namespace MoTool {

using namespace Commands;

TransportBar::TransportBar(EditViewState& evs)
    : viewState_{evs}
    , edit_{evs.edit}
    , transport_{edit_.getTransport()}
{
    transport_.addChangeListener(this);
    transport_.state.addListener(this);
    edit_.state.addListener(this);
    timecodeFormat.referTo(edit_.state, te::IDs::timecodeFormat, &edit_.getUndoManager());

    ::Helpers::addAndMakeVisible (*this, {
        &rewindButton_,
        &playPauseButton_,
        &recordButton_,
        &beatFramesSlider_,
        &bpmValueText_,
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

    beatFramesSlider_.setRange(4, 150, 1);
    beatFramesSlider_.onValueChange = [this] {
        viewState_.setFramesPerBeat((int)beatFramesSlider_.getValue());
    };
    beatFramesSlider_.setIncDecButtonsMode(Slider::incDecButtonsDraggable_Horizontal);

    updateTimeLabels(transport_.getPosition());
}

TransportBar::~TransportBar() {
    transport_.removeChangeListener(this);
    transport_.state.removeListener(this);
    edit_.state.removeListener(this);
}

void TransportBar::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
}

void TransportBar::resized() {
    auto b = getLocalBounds();
    int w = 60;
    bpmValueText_.setBounds(b.removeFromLeft(w * 3 / 2).reduced(2));
    timeSigLabel_.setBounds(b.removeFromLeft(w).reduced(2));

    beatFramesSlider_.setBounds(b.removeFromLeft(w * 3).reduced(2));
    beatFramesSlider_.setTextBoxStyle(Slider::TextBoxLeft, false,
        beatFramesSlider_.getWidth() - w - 4,
        beatFramesSlider_.getTextBoxHeight()
    );

    rewindButton_.setBounds(b.removeFromLeft(w).reduced(2));
    playPauseButton_.setBounds(b.removeFromLeft(w).reduced(2));
    recordButton_.setBounds(b.removeFromLeft(w).reduced(2));
    transportReadout_.setBounds(b.removeFromLeft(w * 2).reduced(2));
    masterVolumeSlider_.setBounds(b.removeFromRight(w).expanded(4, 4));
}

void TransportBar::changeListenerCallback(ChangeBroadcaster*) {
    // TODO use flagged async updater to avoid too many updates
    updatePlayButtonText(transport_.isPlaying());
    updateRecordButtonText(transport_.isRecording());
    updateTimeLabels(transport_.getPosition());
}

void TransportBar::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree.hasType(te::IDs::TEMPO) || tree.hasType(te::IDs::TIMESIG)) {
        updateTimeLabels(transport_.getPosition());
    } else if (tree == transport_.state && prop == te::IDs::position) {
        updateTimeLabels(transport_.getPosition());
    } else if (tree == edit_.state && prop == te::IDs::timecodeFormat) {
        // timecodeFormat = TimecodeDisplayFormatExt::fromString(edit_.state[te::IDs::timecodeFormat].toString());
    }
}

void TransportBar::valueTreeChildAdded(ValueTree&, ValueTree& child) {
    if (child.hasType(te::IDs::TEMPO) || child.hasType(te::IDs::TIMESIG)) {
        updateTimeLabels(transport_.getPosition());
    }
}

void TransportBar::valueTreeChildRemoved(ValueTree&, ValueTree& child, int) {
    if (child.hasType(te::IDs::TEMPO) || child.hasType(te::IDs::TIMESIG)) {
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
    auto& ts = edit_.tempoSequence;
    auto timecode = getTimecode(pos);
    transportReadout_.setText("Pos: " + timecode, dontSendNotification);

    // TODO FPS control

    bpmValueText_.setText(String::formatted("%.2f BPM", ts.getBpmAt(pos)), dontSendNotification);
    timeSigLabel_.setText(ts.getTimeSigAt(pos).getStringTimeSig(), dontSendNotification);
    beatFramesSlider_.setValue(viewState_.getFramesPerBeat(), dontSendNotification);
    beatFramesSlider_.setTextValueSuffix(" frames/beat");
}

} // namespace MoTool
