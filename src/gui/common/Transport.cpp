#include "Transport.h"

#include "../../controllers/MainCommands.h"
#include "../../models/Timecode.h"
#include "LookAndFeel.h"
#include "../../utils/StringLiterals.h"

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
    edit_.getAutomationRecordManager().addListener(this);
    timecodeFormat.referTo(edit_.state, te::IDs::timecodeFormat, &edit_.getUndoManager());

    ::Helpers::addAndMakeVisible (*this, {
        &rewindButton_,
        &playPauseButton_,
        &recordButton_,
        &automationLabel_,
        &autoReadButton_,
        &autoWriteButton_,
        &bpmSlider_,
        &beatFramesSlider_,
        &timeSigLabel_,
        &transportReadout_,
        &masterVolumeSlider_
    });
    masterVolumeSlider_.setPopupDisplayEnabled(true, true, nullptr);
    masterVolumeSlider_.setNumDecimalPlacesToDisplay(2);

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

    autoReadButton_.setClickingTogglesState(true);
    autoReadButton_.onClick = [this] {
        auto& arm = edit_.getAutomationRecordManager();
        arm.setReadingAutomation(!arm.isReadingAutomation());
    };

    autoWriteButton_.setClickingTogglesState(true);
    autoWriteButton_.setColour(TextButton::buttonOnColourId, Colours::red);
    autoWriteButton_.onClick = [this] {
        auto& arm = edit_.getAutomationRecordManager();
        arm.setWritingAutomation(!arm.isWritingAutomation());
    };

    updatePlayButtonText(transport_.isPlaying());
    updateRecordButtonText(transport_.isRecording());
    updateAutomationButtons();

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

    recordButton_.setColour(TextButton::textColourOnId, Colours::red);

    // Apply ReadoutLookAndFeel to all numeric controls
    bpmSlider_.setLookAndFeel(&readoutLookAndFeel_);
    beatFramesSlider_.setLookAndFeel(&readoutLookAndFeel_);
    readoutLookAndFeel_.setupReadoutLabel(timeSigLabel_);
    readoutLookAndFeel_.setupReadoutLabel(transportReadout_);

    automationLabel_.setText("Automation:", dontSendNotification);
    automationLabel_.setJustificationType(Justification::centredRight);

    updateTimeLabels(transport_.getPosition());
}

TransportBar::~TransportBar() {
    bpmSlider_.setLookAndFeel(nullptr);
    beatFramesSlider_.setLookAndFeel(nullptr);

    transport_.removeChangeListener(this);
    transport_.state.removeListener(this);
    edit_.state.removeListener(this);
    edit_.getAutomationRecordManager().removeListener(this);
}

void TransportBar::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
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
    static constexpr int spacing = 8;
    b.reduce(spacing, spacing);
    int w = b.getHeight();

    bpmSlider_.setBounds(b.removeFromLeft(static_cast<int>(w * 6)));
    b.removeFromLeft(spacing);
    timeSigLabel_.setBounds(b.removeFromLeft(w * 2));
    b.removeFromLeft(spacing);
    beatFramesSlider_.setBounds(b.removeFromLeft(static_cast<int>(w * 7)));

    setIncDecSliderStyleWithHeight(bpmSlider_);
    setIncDecSliderStyleWithHeight(beatFramesSlider_);

    b.removeFromLeft(spacing);

    rewindButton_.setBounds(b.removeFromLeft(w * 2));
    b.removeFromLeft(spacing);
    playPauseButton_.setBounds(b.removeFromLeft(w * 2));
    b.removeFromLeft(spacing);
    recordButton_.setBounds(b.removeFromLeft(w * 2));

    b.removeFromLeft(spacing);

    transportReadout_.setBounds(b.removeFromLeft(static_cast<int>(w * 6)));
    b.removeFromLeft(spacing);
    automationLabel_.setBounds(b.removeFromLeft(w * 3));
    b.removeFromLeft(spacing);
    autoReadButton_.setBounds(b.removeFromLeft(w * 2));
    b.removeFromLeft(spacing);
    autoWriteButton_.setBounds(b.removeFromLeft(w * 2));

    //----------------------------------------------------------------------
    // shift everything to the center
    auto shiftBy = b.getWidth() / 2;
    bpmSlider_.setBounds(bpmSlider_.getBounds().withX(bpmSlider_.getX() + shiftBy));
    beatFramesSlider_.setBounds(beatFramesSlider_.getBounds().withX(beatFramesSlider_.getX() + shiftBy));
    timeSigLabel_.setBounds(timeSigLabel_.getBounds().withX(timeSigLabel_.getX() + shiftBy));
    rewindButton_.setBounds(rewindButton_.getBounds().withX(rewindButton_.getX() + shiftBy));
    playPauseButton_.setBounds(playPauseButton_.getBounds().withX(playPauseButton_.getX() + shiftBy));
    recordButton_.setBounds(recordButton_.getBounds().withX(recordButton_.getX() + shiftBy));
    transportReadout_.setBounds(transportReadout_.getBounds().withX(transportReadout_.getX() + shiftBy));
    automationLabel_.setBounds(automationLabel_.getBounds().withX(automationLabel_.getX() + shiftBy));
    autoReadButton_.setBounds(autoReadButton_.getBounds().withX(autoReadButton_.getX() + shiftBy));
    autoWriteButton_.setBounds(autoWriteButton_.getBounds().withX(autoWriteButton_.getX() + shiftBy));

    masterVolumeSlider_.setBounds(b.removeFromRight(w + 8).expanded(4, 4));

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

void TransportBar::automationModeChanged() {
    updateAutomationButtons();
}

void TransportBar::updatePlayButtonText(bool isPlaying) {
    playPauseButton_.setButtonText(isPlaying ? "⏸"_u : "▶"_u);
}

void TransportBar::updateRecordButtonText(bool isRecording) {
    recordButton_.setButtonText(isRecording ? "⏹"_u : "⏺"_u);
    recordButton_.setColour(TextButton::buttonOnColourId, Colours::red);
    // recordButton_.setColour(TextButton::buttonOnColourId, isRecording ?
    //                         recordButton_.findColour(TextButton::textColourOnId)
    //                         : Colours::red);
}

void TransportBar::updateAutomationButtons() {
    auto& arm = edit_.getAutomationRecordManager();
    autoReadButton_.setToggleState(arm.isReadingAutomation(), dontSendNotification);
    autoWriteButton_.setToggleState(arm.isWritingAutomation(), dontSendNotification);
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
