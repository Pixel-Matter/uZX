#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"

#include "Icons.h"
#include "LevelMeter.h"
#include "ParamBindings.h"
#include "LookAndFeel.h"

namespace MoTool {


/** Click-to-type numeric readout, no drag. Displays a number and pushes edits
    through a commit callback; the unit is shown by a separate adjacent label. */
class EditableReadout : public Label {
public:
    EditableReadout();

    /** Called with the parsed number the user typed. Return the value that should
        actually be shown afterwards (clamped/snapped), which becomes the readout. */
    std::function<double(double entered)> onCommit;

    /** Refreshes the displayed value without firing onCommit. */
    void setValue(double value, int decimals);

private:
    void commitEditedText();

    int decimals_ = 0;
    double lastValue_ = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditableReadout)
};


struct TransportBarOptions {
    bool showRecord = true;
    bool showAutomation = true;
};

class TransportBar  : public Component,
                      private ValueTree::Listener,
                      private ChangeListener,
                      private ZoomViewState::Listener,
                      private tracktion::AutomationRecordManager::Listener
{
public:
    explicit TransportBar(EditViewState& evs, TransportBarOptions opts = {});
    ~TransportBar() override;

    /** Sizes and gaps are on a 4 px grid. The transport buttons fill the bar's height inside the margin;
        readouts and meters are shorter. */
    static constexpr int grid = 4;
    static constexpr int margin = 2 * grid, buttonSize = 8 * grid, readoutHeight = 6 * grid;
    static constexpr int height = buttonSize + 2 * margin;

    void paint(Graphics& g) override;
    void resized() override;
    void mouseDown(const MouseEvent&) override;

private:
    /** Lit while Follow is on, dim while suspended; its icon shows by page or by scroll. A click invokes Follow,
        a right-click offers Follow by Page or by Scroll: a command trigger would invoke on right-clicks too. */
    struct FollowButton : IconButton
    {
        using IconButton::IconButton;
        std::function<void(const ModifierKeys&)> onClickWithModifiers;
        void clicked(const ModifierKeys& mods) override {
            if (onClickWithModifiers)
                onClickWithModifiers(mods);
        }
    };

    TransportBarOptions options_;
    EditViewState& viewState_;
    te::Edit& edit_;

    te::TransportControl& transport_;

    Slider masterVolumeSlider_ { Slider::SliderStyle::RotaryVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox };
    SliderParamEndpointBinding masterAttachment_ {masterVolumeSlider_, edit_.getMasterSliderPosParameter()};
    MasterLevelMeter masterMeter_ {edit_};
    CpuLoadMeter cpuMeter_ {edit_.engine};

    IconButton rewindButton_    { "Rewind", Icons::toStart() },
               playPauseButton_ { "Play", Icons::play() },
               recordButton_    { "Record", Icons::record() },
               loopButton_      { "Loop", Icons::loop() };  // lit while looping
    FollowButton followButton_  { "Follow", Icons::followScroll() };
    TextButton autoReadButton_  { "Read" },
               autoWriteButton_ { "Write" };

    // Readouts: a value box and its unit label, laid out with the transport buttons in the middle as
    //   [ 125.00 ] bpm  [ 50 ] fps  [ 24 ] fpb  [ 6 6 6 6 ] groove  [ 4/4 ] sig   (buttons)   pos [ 5.2.12 ]
    EditableReadout bpmControl_;
    EditableReadout fpbControl_;
    Label fpsLabel_;        // click opens a popup of allowed rates
    Label divControl_;
    Label timeSigLabel_;    // editable: "3/4", "6/8" or a bare numerator
    Label bpmUnitLabel_, fpsUnitLabel_, fpbUnitLabel_, divUnitLabel_, sigUnitLabel_;

    Label posLabel_;          // "pos" prefix, left of the editable position box
    Label transportReadout_;  // editable timecode; committing it seeks the transport
    Label automationLabel_;
    te::TimePosition lastPosition_ {te::TimePosition::fromSeconds(-1.0)};

    void changeListenerCallback(ChangeBroadcaster*) override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void automationModeChanged() override;
    void viewRangeChanged() override {}
    void followChanged() override;

    void updatePlayButton(bool isPlaying);
    void updateRecordButton(bool isRecording);
    void updateAutomationButtons();
    void updateTimeLabels(te::TimePosition pos);
    void commitEditedPosition();
    void commitEditedDivisions();
    /** The groove's frame counts; without one, as at a prime or small fpb, the beat as one group. */
    void showGroove(int framesPerBeat);
    void commitEditedTimeSig();
    void showFpsMenu();

    ReadoutLookAndFeel readoutLookAndFeel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

}  // namespace MoTool
