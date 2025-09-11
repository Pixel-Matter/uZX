#include "Transport.h"

#include "../../controllers/MainCommands.h"
#include "../../controllers/App.h"
#include "../../models/Timecode.h"
#include "LookAndFeel.h"

#include <common/Utilities.h>


namespace MoTool {

using namespace Commands;


double BpmControl::snapValue(double attemptedValue, DragMode) {
    auto bpm = viewState_.getBpmSnappedToFps(attemptedValue);
    return bpm;
}


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
        &bpmSlider_,
        &beatFramesSlider_,
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

    bpmSlider_.onValueChange = [this] {
        auto bpm = bpmSlider_.getValue();
        viewState_.setBpmSnappedToFps(bpm);
    };
    bpmSlider_.setIncDecButtonsMode(Slider::incDecButtonsDraggable_Horizontal);
    bpmSlider_.setTextValueSuffix(" BPM");

    beatFramesSlider_.setRange(4, 150, 1);
    beatFramesSlider_.onValueChange = [this] {
        viewState_.setFramesPerBeat((int)beatFramesSlider_.getValue());
    };
    beatFramesSlider_.setIncDecButtonsMode(Slider::incDecButtonsDraggable_Horizontal);
    beatFramesSlider_.setTextValueSuffix(" frames/beat");

    const auto& lookAndFeel = MoToolApp::getApp().getLookAndFeel();
    lookAndFeel.setupNumericTextEditor(timeSigLabel_);
    lookAndFeel.setupNumericTextEditor(transportReadout_);

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

static void setIncDecSliderStyleWithHeight(Slider& s) {
    s.setSliderStyle(Slider::SliderStyle::IncDecButtons);
    s.setIncDecButtonsMode(Slider::incDecButtonsDraggable_Horizontal);
    s.setTextBoxStyle(Slider::TextBoxLeft, false,
        s.getWidth() - s.getHeight(),
        s.getHeight()
    );
}

void TransportBar::resized() {
    auto b = getLocalBounds();
    int w = 60;
    b.reduce(4, 4);

    bpmSlider_.setBounds(b.removeFromLeft(static_cast<int>(w * 2.5)));
    b.removeFromLeft(4);
    timeSigLabel_.setBounds(b.removeFromLeft(w));
    b.removeFromLeft(4);
    beatFramesSlider_.setBounds(b.removeFromLeft(static_cast<int>(w * 3)));

    setIncDecSliderStyleWithHeight(bpmSlider_);
    setIncDecSliderStyleWithHeight(beatFramesSlider_);

    b.removeFromLeft(w / 2);

    rewindButton_.setBounds(b.removeFromLeft(w));
    b.removeFromLeft(4);
    playPauseButton_.setBounds(b.removeFromLeft(w));
    b.removeFromLeft(4);
    recordButton_.setBounds(b.removeFromLeft(w));

    b.removeFromLeft(w / 2);

    transportReadout_.setBounds(b.removeFromLeft(static_cast<int>(w * 2.5)));

    masterVolumeSlider_.setBounds(b.removeFromRight(w / 2 + 4).expanded(4, 4));
}

void TransportBar::changeListenerCallback(ChangeBroadcaster*) {
    // TODO use flagged async updater to avoid too many updates
    updatePlayButtonText(transport_.isPlaying());
    updateRecordButtonText(transport_.isRecording());
    // TODO separate updating of tempo, timsig and fps controls
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

    bpmSlider_.setValue(ts.getBpmAt(pos), dontSendNotification);
    timeSigLabel_.setText(ts.getTimeSigAt(pos).getStringTimeSig(), dontSendNotification);
    beatFramesSlider_.setValue(viewState_.getCurrentFramesPerBeat(), dontSendNotification);
}

} // namespace MoTool
