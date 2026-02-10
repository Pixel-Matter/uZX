#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "../../models/Timecode.h"
#include "../../utils/StringLiterals.h"

#include "ParamBindings.h"
#include "LookAndFeel.h"

namespace MoTool {


class BpmControl : public Slider {
public:
    using Slider::Slider;

    BpmControl(EditViewState& evs)
        : Slider(Slider::SliderStyle::IncDecButtons, Slider::TextEntryBoxPosition::TextBoxLeft)
        , viewState_(evs)
    {
        setTextValueSuffix(" BPM");
        setNumDecimalPlacesToDisplay(2);
        setTextBoxIsEditable(true);
        setRange(te::TempoSetting::minBPM, te::TempoSetting::maxBPM, 0.01);
    }

    double snapValue (double attemptedValue, DragMode dragMode) override;

private:
    EditViewState& viewState_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BpmControl)
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

private:
    TransportBarOptions options_;
    EditViewState& viewState_;
    te::Edit& edit_;

    te::TransportControl& transport_;
    CachedValue<TimecodeDisplayFormatExt> timecodeFormat;

    Slider masterVolumeSlider_ { Slider::SliderStyle::RotaryVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox };
    SliderParamEndpointBinding masterAttachment_ {masterVolumeSlider_, edit_.getMasterSliderPosParameter()};

    TextButton rewindButton_    { "⏮"_u },
    //    stepLeftButton_  { "<" },
       playPauseButton_ { "▶"_u },
       recordButton_    { "⏺"_u },
       autoReadButton_  { "Read" },
       autoWriteButton_ { "Write" };
    //    stepRightButton_ { ">" };

    Label timeSigLabel_;
    Label transportReadout_;
    Label automationLabel_;

    BpmControl bpmSlider_ { viewState_ };
    Slider beatFramesSlider_ { Slider::SliderStyle::IncDecButtons, Slider::TextEntryBoxPosition::TextBoxLeft };
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

    ReadoutLookAndFeel readoutLookAndFeel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

}  // namespace MoTool
