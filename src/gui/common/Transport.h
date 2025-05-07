#pragma once

#include <JuceHeader.h>

#include "../../controllers/ParamAttachments.h"

#include "../../models/Timecode.h"
namespace te = tracktion;

namespace MoTool {

class TransportBar  : public Component,
                      private Timer,
                      private ChangeListener
{
public:
    explicit TransportBar(te::Edit& edit);
    ~TransportBar() override = default;

    void paint(Graphics& g) override;
    void resized() override;

private:
    te::Edit& edit_;
    te::TransportControl& transport_;
    CachedValue<TimecodeDisplayFormatExt> timecodeFormat;

    Slider masterVolumeSlider_ { Slider::SliderStyle::Rotary, Slider::TextEntryBoxPosition::NoTextBox };
    ParameterSliderAttachment masterAttachment_ {masterVolumeSlider_, *edit_.getMasterSliderPosParameter()};

    TextButton rewindButton_    { "|<<" },
    //    stepLeftButton_  { "<" },
       playPauseButton_ { "Play" },
       recordButton_    { "Rec" };
    //    stepRightButton_ { ">" };

Label      bpmLabel_      { "BPM",      "BPM:" },
       timeSigLabel_  { "TimeSig",  "Sig:" },
       transportReadout_ { "Position", "Pos:" };
te::TimePosition lastPosition_ {te::TimePosition::fromSeconds(-1.0)};

    void changeListenerCallback(ChangeBroadcaster*) override;
    void timerCallback() override;

    void updatePlayButtonText(bool isPlaying);
    void updateRecordButtonText(bool isRecording);
    String getTimecode(te::TimePosition pos) const;
    void updateTimeLabels(te::TimePosition pos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

}  // namespace MoTool
