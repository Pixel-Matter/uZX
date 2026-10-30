#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "../../utils/StringLiterals.h"

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
                      private tracktion::AutomationRecordManager::Listener
{
public:
    explicit TransportBar(EditViewState& evs, TransportBarOptions opts = {});
    ~TransportBar() override;

    void paint(Graphics& g) override;
    void resized() override;
    void mouseDown(const MouseEvent&) override;

private:
    TransportBarOptions options_;
    EditViewState& viewState_;
    te::Edit& edit_;

    te::TransportControl& transport_;

    Slider masterVolumeSlider_ { Slider::SliderStyle::RotaryVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox };
    SliderParamEndpointBinding masterAttachment_ {masterVolumeSlider_, edit_.getMasterSliderPosParameter()};

    TextButton rewindButton_    { "⏮"_u },
    //    stepLeftButton_  { "<" },
       playPauseButton_ { "▶"_u },
       recordButton_    { "⏺"_u },
       autoReadButton_  { "Read" },
       autoWriteButton_ { "Write" };
    //    stepRightButton_ { ">" };

    // Readout group: value box + adjacent unit label, laid out as
    //   [ BPM ] BPM   [ FPS ] FPS   [ FPB ] FPB   [ DIVS ] DIV   [ 4/4 ] SIG
    EditableReadout bpmControl_;
    EditableReadout fpbControl_;
    Label fpsLabel_;        // click opens a popup of allowed rates
    Label divControl_;
    Label timeSigLabel_;
    Label bpmUnitLabel_, fpsUnitLabel_, fpbUnitLabel_, divUnitLabel_, sigUnitLabel_;

    Label posLabel_;          // "Pos" prefix, left of the editable position box
    Label transportReadout_;  // editable timecode; committing it seeks the transport
    Label automationLabel_;
    te::TimePosition lastPosition_ {te::TimePosition::fromSeconds(-1.0)};

    void changeListenerCallback(ChangeBroadcaster*) override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void automationModeChanged() override;

    void updatePlayButtonText(bool isPlaying);
    void updateRecordButtonText(bool isRecording);
    void updateAutomationButtons();
    String getTimecode(te::TimePosition pos) const;
    void updateTimeLabels(te::TimePosition pos);
    void commitEditedPosition();
    void commitEditedDivisions();
    void showFpsMenu();

    ReadoutLookAndFeel readoutLookAndFeel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

}  // namespace MoTool
