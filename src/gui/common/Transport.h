#pragma once

#include <JuceHeader.h>

#include "../../controllers/ParamAttachments.h"
#include "../../controllers/EditState.h"

#include "../../models/Timecode.h"
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


class TransportBar  : public Component,
                      private ValueTree::Listener,
                      private ChangeListener
{
public:
    explicit TransportBar(EditViewState& evs);
    ~TransportBar() override;

    void paint(Graphics& g) override;
    void resized() override;

private:
    EditViewState& viewState_;
    te::Edit& edit_;

    te::TransportControl& transport_;
    CachedValue<TimecodeDisplayFormatExt> timecodeFormat;

    Slider masterVolumeSlider_ { Slider::SliderStyle::Rotary, Slider::TextEntryBoxPosition::NoTextBox };
    SliderAttachment masterAttachment_ {masterVolumeSlider_, *edit_.getMasterSliderPosParameter()};

    TextButton rewindButton_    { "|<<" },
    //    stepLeftButton_  { "<" },
       playPauseButton_ { "Play" },
       recordButton_    { "Rec" };
    //    stepRightButton_ { ">" };

    Label timeSigLabel_;
    Label transportReadout_;

    BpmControl bpmSlider_ { viewState_ };
    Slider beatFramesSlider_ { Slider::SliderStyle::IncDecButtons, Slider::TextEntryBoxPosition::TextBoxLeft };
    te::TimePosition lastPosition_ {te::TimePosition::fromSeconds(-1.0)};

    void changeListenerCallback(ChangeBroadcaster*) override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;

    void updatePlayButtonText(bool isPlaying);
    void updateRecordButtonText(bool isRecording);
    String getTimecode(te::TimePosition pos) const;
    void updateTimeLabels(te::TimePosition pos);

    ReadoutLookAndFeel readoutLookAndFeel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

}  // namespace MoTool
