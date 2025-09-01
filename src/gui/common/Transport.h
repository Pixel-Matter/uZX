#pragma once

#include <JuceHeader.h>

#include "../../controllers/ParamAttachments.h"
#include "../../controllers/EditState.h"

#include "../../models/Timecode.h"

namespace MoTool {

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

    Label
        bpmValueText_     { "BPMValue",   "120" },
        beatFramesLabel_  { "BeatFrames", "26 frames/beat" },
        timeSigLabel_     { "TimeSig",    "Sig:" },
        transportReadout_ { "Position",   "Pos:" };
    te::TimePosition lastPosition_ {te::TimePosition::fromSeconds(-1.0)};

    void changeListenerCallback(ChangeBroadcaster*) override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;

    void updatePlayButtonText(bool isPlaying);
    void updateRecordButtonText(bool isRecording);
    String getTimecode(te::TimePosition pos) const;
    void updateTimeLabels(te::TimePosition pos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

}  // namespace MoTool
