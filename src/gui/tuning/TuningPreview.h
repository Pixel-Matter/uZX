#pragma once

#include "JuceHeader.h"
#include "../../models/tuning/TuningSystem.h"
#include "../../viewmodels/tuning/TuningViewModel.h"

namespace MoTool {

// TODO introduce ViewModel for TuningPreviewGrid

class TuningPreviewGrid : public juce::Component {
public:
    TuningPreviewGrid(TuningViewModel& vm);
    ~TuningPreviewGrid() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    TuningViewModel& viewModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPreviewGrid)
};


class TuningPreviewComponent : public juce::Component {
public:
    TuningPreviewComponent();
    ~TuningPreviewComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    TuningViewModel viewModel;

    Label ChipClockLabel;
    Label ScaleLabel;  // with Root note
    Label TuningTypeLabel;
    Label TuningNameLabel;
    Label ToneEnvSwitchLabel;

    TuningPreviewGrid tuningGrid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPreviewComponent)
};

}  // namespace MoTool