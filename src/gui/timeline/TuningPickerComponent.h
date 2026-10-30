#pragma once

#include <JuceHeader.h>

#include "../../models/PsgClip.h"

namespace MoTool {

//==============================================================================
/** Lets the user settle a PSG clip's tuning table and key when detection is unsure,
    or override it. Shows the ranked tables and keys with their scores and the clip's
    pitch classes at sub-semitone resolution on the selected grid, so a wrong grid is
    visible as peaks between the lines. Choices are written to the clip state at once;
    "Use detected" removes them. */
class TuningPickerComponent : public Component, private juce::ValueTree::Listener
{
public:
    explicit TuningPickerComponent(PsgClip::Ptr clip);
    ~TuningPickerComponent() override;

    /** Opens the picker for a clip in its own window. */
    static void show(PsgClip& clip);

    void resized() override;

private:
    class HistogramView;

    PsgClip::Ptr clip_;
    PitchHistogram histogram_;

    Label tableLabel_{{}, "Tuning table"};
    Label keyLabel_{{}, "Key"};
    ComboBox tableBox_;
    ComboBox keyBox_;
    TextButton detectedButton_{"Use detected"};
    std::unique_ptr<HistogramView> histogramView_;

    static constexpr int equalTemperamentItemId = 1000;

    void refresh();
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPickerComponent)
};

}  // namespace MoTool
