#include "Transport.h"

#include "../../controllers/MainCommands.h"
#include "../../models/EditUtilities.h"
#include "../../models/Ids.h"
#include "LookAndFeel.h"
#include "../../utils/StringLiterals.h"

#include <common/Utilities.h>


namespace MoTool {

using namespace Commands;


EditableReadout::EditableReadout() {
    setEditable(true, true, false);  // edit on single or double click
    setKeyboardType(TextInputTarget::decimalKeyboard);
    // onTextChange fires after the edited text has been committed to the label,
    // so we read it back from getText() (the editor is already gone by then).
    onTextChange = [this] { commitEditedText(); };
}

void EditableReadout::setValue(double value, int decimals) {
    lastValue_ = value;
    decimals_ = decimals;
    setText(String(value, decimals), dontSendNotification);
}

void EditableReadout::commitEditedText() {
    if (! onCommit) return;

    auto entered = getText().retainCharacters("0123456789.-").getDoubleValue();
    auto resolved = onCommit(entered);
    setValue(resolved, decimals_);
}


TransportBar::TransportBar(EditViewState& evs, TransportBarOptions opts)
    : options_{opts}
    , viewState_{evs}
    , edit_{evs.edit}
    , transport_{edit_.getTransport()}
{
    transport_.addChangeListener(this);
    transport_.state.addListener(this);
    edit_.state.addListener(this);
    edit_.getAutomationRecordManager().addListener(this);

    addAndMakeVisible(rewindButton_);
    addAndMakeVisible(playPauseButton_);
    addAndMakeVisible(bpmControl_);
    addAndMakeVisible(bpmUnitLabel_);
    addAndMakeVisible(fpsLabel_);
    addAndMakeVisible(fpsUnitLabel_);
    addAndMakeVisible(fpbControl_);
    addAndMakeVisible(fpbUnitLabel_);
    addAndMakeVisible(divControl_);
    addAndMakeVisible(divUnitLabel_);
    addAndMakeVisible(timeSigLabel_);
    addAndMakeVisible(sigUnitLabel_);
    addAndMakeVisible(posLabel_);
    addAndMakeVisible(transportReadout_);
    addAndMakeVisible(masterVolumeSlider_);

    if (options_.showRecord)
        addAndMakeVisible(recordButton_);
    if (options_.showAutomation) {
        addAndMakeVisible(automationLabel_);
        addAndMakeVisible(autoReadButton_);
        addAndMakeVisible(autoWriteButton_);
    }

    masterVolumeSlider_.setPopupDisplayEnabled(true, true, nullptr);
    masterVolumeSlider_.setNumDecimalPlacesToDisplay(2);

    if (auto mgr = edit_.engine.getUIBehaviour().getApplicationCommandManager()) {
        rewindButton_.setCommandToTrigger(mgr, MainAppCommands::transportToStart, true);
        playPauseButton_.setCommandToTrigger(mgr, MainAppCommands::transportPlay, true);
    }

    if (options_.showRecord) {
        recordButton_.onClick = [this] {
            bool wasRecording = edit_.getTransport().isRecording();
            if (!wasRecording) {
                edit_.engine.getUIBehaviour().getApplicationCommandManager()->invokeDirectly(MainAppCommands::transportRecord, false);
            } else {
                edit_.engine.getUIBehaviour().getApplicationCommandManager()->invokeDirectly(MainAppCommands::transportRecordStop, false);
            }
        };
        recordButton_.setColour(TextButton::textColourOnId, Colours::red);
    }

    if (options_.showAutomation) {
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

        automationLabel_.setText("Automation:", dontSendNotification);
        automationLabel_.setJustificationType(Justification::centredRight);
    }

    updatePlayButtonText(transport_.isPlaying());
    if (options_.showRecord)
        updateRecordButtonText(transport_.isRecording());
    if (options_.showAutomation)
        updateAutomationButtons();

    // Editable value boxes: commit callbacks clamp/snap and return the value to show.
    bpmControl_.onCommit = [this](double entered) {
        auto bpm = jlimit(te::TempoSetting::minBPM, te::TempoSetting::maxBPM, entered);
        bpm = viewState_.getBpmSnappedToFps(bpm);
        viewState_.setBpmSnappedToFps(bpm);
        return bpm;
    };
    fpbControl_.onCommit = [this](double entered) {
        auto fpb = jlimit(4, 150, roundToInt(entered));
        viewState_.setFramesPerBeat(fpb);
        const auto resolvedFpb = jmax(1, roundToInt(viewState_.getCurrentFramesPerBeat()));
        Helpers::setGridSubdivisionPattern(edit_, Helpers::getDefaultGridSubdivisionPattern(resolvedFpb));
        return resolvedFpb;
    };

    // Apply ReadoutLookAndFeel to all editable/clickable value boxes.
    readoutLookAndFeel_.setupReadoutLabel(bpmControl_);
    readoutLookAndFeel_.setupReadoutLabel(fpbControl_);
    readoutLookAndFeel_.setupReadoutLabel(divControl_);
    readoutLookAndFeel_.setupReadoutLabel(timeSigLabel_);
    readoutLookAndFeel_.setupReadoutLabel(fpsLabel_);
    readoutLookAndFeel_.setupReadoutLabel(transportReadout_);

    // Editing the position box seeks the transport.
    transportReadout_.onTextChange = [this] { commitEditedPosition(); };
    divControl_.setEditable(true, true, false);
    divControl_.onTextChange = [this] { commitEditedDivisions(); };

    // Unit / prefix labels sit next to their value box as plain, non-interactive text.
    struct { Label& label; const char* text; } units[] = {
        { bpmUnitLabel_, "BPM" }, { fpsUnitLabel_, "FPS" },
        { fpbUnitLabel_, "FPB" }, { divUnitLabel_, "DIV" }, { sigUnitLabel_, "SIG" },
        { posLabel_, "Pos" },
    };
    for (auto& u : units) {
        u.label.setText(u.text, dontSendNotification);
        u.label.setFont(readoutLookAndFeel_.getNumericReadoutFont());
        u.label.setColour(Label::textColourId, Colors::Theme::primary);
    }

    // "Pos" is a prefix, so hug it against the timecode box on its right.
    posLabel_.setJustificationType(Justification::centredRight);

    // The fps readout picks from a fixed set of rates via a popup, not free text.
    fpsLabel_.setEditable(false, false, false);
    fpsLabel_.addMouseListener(this, false);

    updateTimeLabels(transport_.getPosition());
}

TransportBar::~TransportBar() {
    fpsLabel_.removeMouseListener(this);

    transport_.removeChangeListener(this);
    transport_.state.removeListener(this);
    edit_.state.removeListener(this);
    edit_.getAutomationRecordManager().removeListener(this);
}

void TransportBar::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
}

void TransportBar::resized() {
    auto b = getLocalBounds();
    static constexpr int spacing = 8;
    b.reduce(spacing, spacing);
    int w = b.getHeight();

    // Components that make up the left-aligned block, collected so we can shift the
    // whole block to horizontal centre afterwards.
    std::vector<Component*> block;

    // A readout is a value box followed by an adjacent unit label.
    auto placeReadout = [&](Component& value, Label& unit, int valueW, int unitW) {
        value.setBounds(b.removeFromLeft(valueW));
        unit.setBounds(b.removeFromLeft(unitW));
        b.removeFromLeft(spacing);
        block.push_back(&value);
        block.push_back(&unit);
    };
    auto placeButton = [&](Component& c, int cw) {
        c.setBounds(b.removeFromLeft(cw));
        b.removeFromLeft(spacing);
        block.push_back(&c);
    };

    placeReadout(bpmControl_, bpmUnitLabel_, w * 3, w * 2);
    placeReadout(fpsLabel_,   fpsUnitLabel_, w * 2, w * 2);
    placeReadout(fpbControl_, fpbUnitLabel_, w * 2, w * 2);
    placeReadout(divControl_, divUnitLabel_, w * 3, w * 2);
    placeReadout(timeSigLabel_, sigUnitLabel_, w * 2, w * 2);

    placeButton(rewindButton_, w * 2);
    placeButton(playPauseButton_, w * 2);
    if (options_.showRecord)
        placeButton(recordButton_, w * 2);

    // Position readout: "Pos" prefix label, then the editable timecode box.
    posLabel_.setBounds(b.removeFromLeft(static_cast<int>(w * 2)));
    transportReadout_.setBounds(b.removeFromLeft(static_cast<int>(w * 5)));
    b.removeFromLeft(spacing);
    block.push_back(&posLabel_);
    block.push_back(&transportReadout_);

    if (options_.showAutomation) {
        placeButton(automationLabel_, w * 3);
        placeButton(autoReadButton_, w * 2);
        placeButton(autoWriteButton_, w * 2);
    }

    //----------------------------------------------------------------------
    // shift the whole block to the horizontal centre of the remaining space
    auto shiftBy = b.getWidth() / 2;
    for (auto* c : block)
        c->setBounds(c->getBounds().withX(c->getX() + shiftBy));

    masterVolumeSlider_.setBounds(b.removeFromRight(w + 8).expanded(4, 4));
}

void TransportBar::changeListenerCallback(ChangeBroadcaster*) {
    // TODO use flagged async updater to avoid too many updates
    updatePlayButtonText(transport_.isPlaying());
    if (options_.showRecord)
        updateRecordButtonText(transport_.isRecording());
    // TODO separate updating of tempo, timsig and fps controls
    updateTimeLabels(transport_.getPosition());
}

void TransportBar::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree.hasType(te::IDs::TEMPO) || tree.hasType(te::IDs::TIMESIG)) {
        updateTimeLabels(transport_.getPosition());
    } else if (tree == transport_.state && prop == te::IDs::position) {
        updateTimeLabels(transport_.getPosition());
    } else if (tree == edit_.state && prop == IDs::timecodeDisplayMode) {
        updateTimeLabels(transport_.getPosition());
    } else if (tree == edit_.state && prop == IDs::projectFps) {
        updateTimeLabels(transport_.getPosition());
    } else if (tree == edit_.state && prop == IDs::gridSubdivision) {
        updateTimeLabels(transport_.getPosition());
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
    recordButton_.setColour(TextButton::textColourOnId, Colours::red);
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
    return te::TimecodeDisplayFormat::toFullTimecode(pos, 100, true);
}

void TransportBar::updateTimeLabels(te::TimePosition pos) {
    auto& ts = edit_.tempoSequence;
    transportReadout_.setText(getTimecode(pos), dontSendNotification);

    bpmControl_.setValue(ts.getBpmAt(pos), 2);
    timeSigLabel_.setText(ts.getTimeSigAt(pos).getStringTimeSig(), dontSendNotification);
    const auto framesPerBeat = jmax(1, roundToInt(viewState_.getCurrentFramesPerBeat()));
    fpbControl_.setValue(framesPerBeat, 0);
    divControl_.setText(Helpers::formatGridSubdivisionPattern(
                            Helpers::getGridSubdivisionPattern(edit_, framesPerBeat)),
                        dontSendNotification);
    fpsLabel_.setText(String(roundToInt(Helpers::getProjectFps(edit_))), dontSendNotification);
}

void TransportBar::commitEditedPosition() {
    // Parse the same layout getTimecode() emits: HH:MM:SS:CC (centiseconds),
    // shorter inputs are read from the least-significant end (e.g. "SS:CC" or "SS").
    auto parts = StringArray::fromTokens(transportReadout_.getText().trim(), ":", "");
    parts.trim();
    parts.removeEmptyStrings();
    if (parts.isEmpty()) {
        updateTimeLabels(transport_.getPosition());  // restore display
        return;
    }

    double hours = 0, mins = 0, secs = 0, centis = 0;
    switch (parts.size()) {
        case 1: secs = parts[0].getDoubleValue(); break;
        case 2: secs = parts[0].getIntValue(); centis = parts[1].getIntValue(); break;
        case 3: mins = parts[0].getIntValue(); secs = parts[1].getIntValue(); centis = parts[2].getIntValue(); break;
        default:
            hours = parts[0].getIntValue(); mins = parts[1].getIntValue();
            secs = parts[2].getIntValue(); centis = parts[3].getIntValue();
            break;
    }

    auto totalSeconds = jmax(0.0, hours * 3600.0 + mins * 60.0 + secs + centis / 100.0);
    transport_.setPosition(te::TimePosition::fromSeconds(totalSeconds));
    updateTimeLabels(transport_.getPosition());  // re-normalise the display
}

void TransportBar::commitEditedDivisions() {
    const auto framesPerBeat = jmax(1, roundToInt(viewState_.getCurrentFramesPerBeat()));
    const auto pattern = Helpers::parseGridSubdivisionPattern(divControl_.getText(), framesPerBeat);
    Helpers::setGridSubdivisionPattern(edit_, pattern);
    divControl_.setText(Helpers::formatGridSubdivisionPattern(
                            Helpers::getGridSubdivisionPattern(edit_, framesPerBeat)),
                        dontSendNotification);
}

void TransportBar::mouseDown(const MouseEvent& e) {
    if (e.eventComponent == &fpsLabel_)
        showFpsMenu();
}

void TransportBar::showFpsMenu() {
    const auto currentFps = roundToInt(Helpers::getProjectFps(edit_));

    PopupMenu menu;
    for (double fps : Helpers::kAllowedProjectFps) {
        const int rounded = roundToInt(fps);
        menu.addItem(String(rounded) + " fps", true, rounded == currentFps,
                     [this, fps] { Helpers::setProjectFps(edit_, fps); });
    }
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&fpsLabel_));
}

} // namespace MoTool
