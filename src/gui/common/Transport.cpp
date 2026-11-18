#include "Transport.h"

#include "../../controllers/MainCommands.h"
#include "../../models/EditUtilities.h"
#include "../../models/Ids.h"
#include "../../models/PsgFrameRetimer.h"
#include "../../models/PsgTempoDetector.h"
#include "LookAndFeel.h"

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


/** Where `label` puts its text's baseline, from its top, as LookAndFeel_V2::drawLabel lays the text out. */
static float getTextBaseline(Label& label) {
    auto& lf = label.getLookAndFeel();
    const auto area = lf.getLabelBorderSize(label).subtractedFrom(label.getLocalBounds()).toFloat();
    GlyphArrangement glyphs;
    glyphs.addFittedText(lf.getLabelFont(label), "0", area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                         label.getJustificationType(), 1, label.getMinimumHorizontalScale());
    return glyphs.getNumGlyphs() > 0 ? glyphs.getGlyph(0).getBaselineY() : area.getBottom();
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
    addAndMakeVisible(loopButton_);
    addAndMakeVisible(followButton_);
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
    addAndMakeVisible(masterMeter_);
    addAndMakeVisible(cpuMeter_);

    if (options_.showRecord)
        addAndMakeVisible(recordButton_);
    if (options_.showAutomation) {
        addAndMakeVisible(automationLabel_);
        addAndMakeVisible(autoReadButton_);
        addAndMakeVisible(autoWriteButton_);
    }

    masterVolumeSlider_.setNumDecimalPlacesToDisplay(2);

    if (auto mgr = edit_.engine.getUIBehaviour().getApplicationCommandManager()) {
        rewindButton_.setCommandToTrigger(mgr, MainAppCommands::transportToStart, true);
        loopButton_.setCommandToTrigger(mgr, MainAppCommands::transportLoop, true);
    }
    followButton_.setTooltip("Follow the playhead during playback (right-click: by Page or by Scroll)");
    followButton_.onClickWithModifiers = [this](const ModifierKeys& mods) {
        auto* mgr = edit_.engine.getUIBehaviour().getApplicationCommandManager();
        if (mgr == nullptr)
            return;
        if (!mods.isPopupMenu()) {
            mgr->invokeDirectly(MainAppCommands::transportFollow, true);
            return;
        }
        PopupMenu menu;
        menu.addCommandItem(mgr, MainAppCommands::transportFollowByPage);
        menu.addCommandItem(mgr, MainAppCommands::transportFollowByScroll);
        menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&followButton_));
    };
    viewState_.zoom.addListener(this);
    followChanged();

    if (options_.showRecord) {
        recordButton_.onClick = [this] {
            bool wasRecording = edit_.getTransport().isRecording();
            if (!wasRecording) {
                edit_.engine.getUIBehaviour().getApplicationCommandManager()->invokeDirectly(MainAppCommands::transportRecord, false);
            } else {
                edit_.engine.getUIBehaviour().getApplicationCommandManager()->invokeDirectly(MainAppCommands::transportRecordStop, false);
            }
        };
        recordButton_.setColour(TextButton::textColourOnId, Colors::Theme::error);
    }

    if (options_.showAutomation) {
        autoReadButton_.setClickingTogglesState(true);
        autoReadButton_.onClick = [this] {
            auto& arm = edit_.getAutomationRecordManager();
            arm.setReadingAutomation(!arm.isReadingAutomation());
        };

        autoWriteButton_.setClickingTogglesState(true);
        autoWriteButton_.setColour(TextButton::buttonOnColourId, Colors::Theme::error);
        autoWriteButton_.onClick = [this] {
            auto& arm = edit_.getAutomationRecordManager();
            arm.setWritingAutomation(!arm.isWritingAutomation());
        };

    }

    loopButton_.setToggleState(transport_.looping, dontSendNotification);
    if (options_.showRecord)
        updateRecordButton(transport_.isRecording());
    if (options_.showAutomation)
        updateAutomationButtons();

    // Editable value boxes: commit callbacks clamp/snap and return the value to show.
    bpmControl_.onCommit = [this](double entered) {
        const auto bpm = jlimit(te::TempoSetting::minBPM, te::TempoSetting::maxBPM, entered);
        return PsgTiming::setBpmSnappedToFrames(edit_, bpm, viewState_.shouldPreservePsgTimingOnTempoChange());
    };
    fpbControl_.onCommit = [this](double entered) {
        auto fpb = jlimit(4, 150, roundToInt(entered));
        PsgTiming::setFramesPerBeat(edit_, fpb, viewState_.shouldPreservePsgTimingOnTempoChange());
        const auto resolvedFpb = jmax(1, roundToInt(PsgTiming::getFramesPerBeat(edit_)));
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
    // Values end at their units, on their right; the position's label is on its left
    for (auto* value : std::initializer_list<Label*> {&bpmControl_, &fpsLabel_, &fpbControl_, &divControl_, &timeSigLabel_})
        value->setJustificationType(Justification::right);

    // Editing the position box seeks the transport.
    transportReadout_.onTextChange = [this] { commitEditedPosition(); };
    divControl_.setEditable(true, true, false);
    divControl_.onTextChange = [this] { commitEditedDivisions(); };
    timeSigLabel_.setEditable(true, true, false);
    timeSigLabel_.onTextChange = [this] { commitEditedTimeSig(); };

    // Unit and prefix labels: small and grey, so the values and the buttons stand out
    struct { Label& label; const char* text; } units[] = {
        { bpmUnitLabel_, "bpm" }, { fpsUnitLabel_, "fps" }, { fpbUnitLabel_, "fpb" },
        { divUnitLabel_, "groove" }, { sigUnitLabel_, "sig" }, { posLabel_, "pos" },
        { automationLabel_, "Automation" },
    };
    for (auto& u : units) {
        u.label.setText(u.text, dontSendNotification);
        u.label.setFont(FontOptions(15.0f));
        u.label.setColour(Label::textColourId, Colors::Theme::textSecondary);
        u.label.setBorderSize({});  // laid out at the width of its text
    }

    // Each value and its unit say what the value is and how to change it: nothing else shows they can be
    struct { Label& value; Label& unit; const char* tip; } tips[] = {
        { bpmControl_, bpmUnitLabel_, "Tempo in beats per minute: click to type another" },
        { fpsLabel_, fpsUnitLabel_, "Frames per second: click to pick another rate" },
        { fpbControl_, fpbUnitLabel_, "Frames per beat: click to type another, from 4 to 150" },
        { divControl_, divUnitLabel_, "Frames in each group of a beat, such as 6 6 6 6: click to type your own" },
        { timeSigLabel_, sigUnitLabel_, "Time signature: click to type another, such as 3/4 or 6/8" },
        { transportReadout_, posLabel_, "Playhead position: click to type one and jump there" },
    };
    for (auto& t : tips) {
        t.value.setTooltip(t.tip);
        t.unit.setTooltip(t.tip);
    }

    // The fps readout picks from a fixed set of rates via a popup, not free text.
    fpsLabel_.setEditable(false, false, false);
    fpsLabel_.addMouseListener(this, false);

    updateTimeLabels(transport_.getPosition());
}

TransportBar::~TransportBar() {
    fpsLabel_.removeMouseListener(this);

    viewState_.zoom.removeListener(this);
    transport_.removeChangeListener(this);
    transport_.state.removeListener(this);
    edit_.state.removeListener(this);
    edit_.getAutomationRecordManager().removeListener(this);
}

void TransportBar::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
}

void TransportBar::resized() {
    static constexpr int spacing = margin;
    auto b = getLocalBounds().reduced(margin);
    const int w = readoutHeight;  // the unit of the readouts' widths

    // Output section on the right, reserved first so the centred block can't overlap it.
    // ponytail: 60 px puts the knob centre 92 px from the right edge, midway between the Player's
    // AY panel Width and Volume knobs; re-derive if either layout changes.
    static constexpr int meterWidth = 15 * grid;
    masterMeter_.setBounds(b.removeFromRight(meterWidth).withSizeKeepingCentre(meterWidth, b.getHeight() / 2));
    b.removeFromRight(spacing);
    static constexpr int knobWidth = 8 * grid;
    masterVolumeSlider_.setBounds(
        b.removeFromRight(knobWidth).withSizeKeepingCentre(knobWidth, w).expanded(grid, grid));
    b.removeFromRight(spacing);
    static constexpr int cpuMeterWidth = 18 * grid;
    cpuMeter_.setBounds(b.removeFromRight(cpuMeterWidth));
    b.removeFromRight(spacing);

    // The block is laid out from x = 0, then shifted into place. Gaps show what belongs together:
    // a label hugs its value, readouts stand apart, groups further apart, the transport buttons close.
    static constexpr int labelGap = grid, buttonGap = 2 * grid, readoutGap = 3 * grid, groupGap = 6 * grid;
    std::vector<Component*> block;
    int x = 0;
    // Readouts and labels are readoutHeight tall, centred on the bar
    auto place = [&](Component& c, int width, int gapAfter) {
        c.setBounds(Rectangle<int>(x, b.getY(), width, b.getHeight()).withSizeKeepingCentre(width, w));
        x += width + gapAfter;
        block.push_back(&c);
    };
    // Transport buttons are squares of the bar's height
    auto placeButton = [&](Component& c, int gapAfter) {
        c.setBounds(x, b.getY(), b.getHeight(), b.getHeight());
        x += b.getHeight() + gapAfter;
        block.push_back(&c);
    };
    auto textWidth = [](const Label& l) { return GlyphArrangement::getStringWidthInt(l.getFont(), l.getText()) + 1; };
    // A label's text sits on its value's baseline, though the two fonts sit differently in their boxes
    auto alignBaseline = [](Label& label, Label& value) {
        label.setTopLeftPosition(label.getX(),
                                 value.getY() + roundToInt(getTextBaseline(value) - getTextBaseline(label)));
    };
    auto placeReadout = [&](Label& value, int valueW, Label& unit, int gapAfter) {
        place(value, valueW, labelGap);
        place(unit, textWidth(unit), gapAfter);
        alignBaseline(unit, value);
    };

    placeReadout(bpmControl_, w * 3, bpmUnitLabel_, readoutGap);
    placeReadout(fpsLabel_, w * 2, fpsUnitLabel_, readoutGap);
    placeReadout(fpbControl_, w * 2, fpbUnitLabel_, readoutGap);
    placeReadout(divControl_, w * 3, divUnitLabel_, readoutGap);
    placeReadout(timeSigLabel_, w * 2, sigUnitLabel_, groupGap);

    const int buttonsStart = x;
    placeButton(rewindButton_, buttonGap);
    placeButton(playPauseButton_, buttonGap);
    if (options_.showRecord)
        placeButton(recordButton_, buttonGap);
    placeButton(loopButton_, buttonGap);
    placeButton(followButton_, 0);
    const int buttonsEnd = x;
    x += groupGap;

    place(posLabel_, textWidth(posLabel_), labelGap);  // a prefix, before its value
    place(transportReadout_, w * 5, 0);
    alignBaseline(posLabel_, transportReadout_);

    if (options_.showAutomation) {
        x += groupGap;
        place(automationLabel_, textWidth(automationLabel_), labelGap);
        place(autoReadButton_, w * 2, buttonGap);
        place(autoWriteButton_, w * 2, 0);
    }

    // The transport buttons sit in the middle of the whole bar. Where the block doesn't fit there, it moves
    // to stay clear of the output section first, so play/pause and the position stay visible.
    auto shift = getLocalBounds().getCentreX() - (buttonsStart + buttonsEnd) / 2;
    shift = jmax(shift, b.getX());
    shift = jmin(shift, b.getRight() - x);
    for (auto* c : block)
        c->setBounds(c->getBounds().translated(shift, 0));
}

void TransportBar::changeListenerCallback(ChangeBroadcaster*) {
    // TODO use flagged async updater to avoid too many updates
    if (options_.showRecord)
        updateRecordButton(transport_.isRecording());
    // TODO separate updating of tempo, timsig and fps controls
    updateTimeLabels(transport_.getPosition());
}

void TransportBar::valueTreePropertyChanged(ValueTree& tree, const Identifier& prop) {
    if (tree.hasType(te::IDs::TEMPO) || tree.hasType(te::IDs::TIMESIG)) {
        updateTimeLabels(transport_.getPosition());
    } else if (tree == transport_.state && prop == te::IDs::position) {
        updateTimeLabels(transport_.getPosition());
    } else if (tree == transport_.state && prop == te::IDs::looping) {
        // From the tree: the transport's cached value may not have caught up yet
        loopButton_.setToggleState(static_cast<bool>(tree[prop]), dontSendNotification);
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

void TransportBar::followChanged() {
    const auto& zoom = viewState_.zoom;
    followButton_.setIcon(zoom.followsByPage() ? Icons::followPage() : Icons::followScroll());
    followButton_.setToggleState(zoom.followsPlayback(), dontSendNotification);
    followButton_.setColour(TextButton::buttonOnColourId,
                            zoom.isFollowSuspended() ? Colors::Theme::primary.withAlpha(0.35f) : Colors::Theme::primary);
}

PlayButton::PlayButton(te::Edit& edit) : IconButton("Play", Icons::play()), edit_(&edit) {
    if (auto* mgr = edit.engine.getUIBehaviour().getApplicationCommandManager())
        setCommandToTrigger(mgr, MainAppCommands::transportPlay, true);
    edit.getTransport().addChangeListener(this);
    showState();
}

PlayButton::~PlayButton() {
    if (auto* edit = edit_.get())
        edit->getTransport().removeChangeListener(this);
}

void PlayButton::changeListenerCallback(ChangeBroadcaster*) {
    if (auto* edit = edit_.get(); edit != nullptr && edit->getTransport().isPlaying() != playing_)
        showState();
}

void PlayButton::showState() {
    auto* edit = edit_.get();
    playing_ = edit != nullptr && edit->getTransport().isPlaying();
    setIcon(playing_ ? Icons::pause() : Icons::play());
    // Lit while playing, in the on colours of a toggle such as Loop. Colours, not the toggle state: the button
    // triggers a command, and JUCE resets a command button's toggle state to the command's tick.
    if (playing_) {
        setColour(TextButton::buttonColourId, findColour(TextButton::buttonOnColourId));
        setColour(TextButton::textColourOffId, findColour(TextButton::textColourOnId));
    } else {
        removeColour(TextButton::buttonColourId);
        removeColour(TextButton::textColourOffId);
    }
}

void TransportBar::updateRecordButton(bool isRecording) {
    recordButton_.setIcon(isRecording ? Icons::stop() : Icons::record());
    recordButton_.setColour(TextButton::textColourOnId, Colors::Theme::error);
    // recordButton_.setColour(TextButton::buttonOnColourId, isRecording ?
    //                         recordButton_.findColour(TextButton::textColourOnId)
    //                         : Colours::red);
}

void TransportBar::updateAutomationButtons() {
    auto& arm = edit_.getAutomationRecordManager();
    autoReadButton_.setToggleState(arm.isReadingAutomation(), dontSendNotification);
    autoWriteButton_.setToggleState(arm.isWritingAutomation(), dontSendNotification);
}

void TransportBar::updateTimeLabels(te::TimePosition pos) {
    auto& ts = edit_.tempoSequence;
    transportReadout_.setText(Helpers::makeSnapLadder(edit_, pos).getPositionLabel(pos, ts), dontSendNotification);

    bpmControl_.setValue(ts.getBpmAt(pos), 2);
    timeSigLabel_.setText(ts.getTimeSigAt(pos).getStringTimeSig(), dontSendNotification);
    const auto framesPerBeat = jmax(1, roundToInt(PsgTiming::getFramesPerBeat(edit_)));
    fpbControl_.setValue(framesPerBeat, 0);
    showGroove(framesPerBeat);
    fpsLabel_.setText(String(roundToInt(Helpers::getProjectFps(edit_))), dontSendNotification);
}

void TransportBar::commitEditedPosition() {
    const auto pos = transport_.getPosition();
    if (auto parsed = Helpers::makeSnapLadder(edit_, pos).parsePosition(transportReadout_.getText(), edit_.tempoSequence))
        transport_.setPosition(*parsed);
    updateTimeLabels(transport_.getPosition());  // shows the result, or restores the old one
}

void TransportBar::commitEditedDivisions() {
    const auto framesPerBeat = jmax(1, roundToInt(PsgTiming::getFramesPerBeat(edit_)));
    const auto pattern = Helpers::parseGridSubdivisionPattern(divControl_.getText(), framesPerBeat);
    Helpers::setGridSubdivisionPattern(edit_, pattern);
    showGroove(framesPerBeat);
}

void TransportBar::showGroove(int framesPerBeat) {
    // One group is no groove, as typing a single number sets: an empty box would look broken
    auto pattern = Helpers::getGridSubdivisionPattern(edit_, framesPerBeat);
    if (pattern.empty())
        pattern = {framesPerBeat};
    divControl_.setText(Helpers::formatGridSubdivisionPattern(pattern), dontSendNotification);
}

void TransportBar::commitEditedTimeSig() {
    auto& timeSig = edit_.tempoSequence.getTimeSigAt(transport_.getPosition());
    if (const auto parsed = Helpers::parseTimeSig(timeSigLabel_.getText(), timeSig.denominator.get()))
        PsgTiming::setTimeSig(edit_, timeSig, parsed->first, parsed->second);
    updateTimeLabels(transport_.getPosition());  // shows the result, or restores the old one
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
                     [this, fps] { PsgTempo::setProjectFrameRate(edit_, fps); });
    }
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&fpsLabel_));
}

} // namespace MoTool
