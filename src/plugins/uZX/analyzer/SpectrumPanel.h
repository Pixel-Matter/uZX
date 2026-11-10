#pragma once

#include <JuceHeader.h>

#include <optional>

#include "../../../controllers/EditState.h"
#include "SpectrumAnalyzerPlugin.h"

namespace MoTool::uZX {

//==============================================================================
/**
 * Viz mode's spectrum analyzer: 1/3-octave bars of the master mix with falling peak caps, in a panel at the left
 * of the window, the lowest band at the bottom and the bars growing to the left from the notes, or along the
 * bottom, the lowest band at the left and the bars growing up. The gap on its side towards the notes drags to
 * resize it. No bars while the master has no SpectrumAnalyzerPlugin.
 */
class SpectrumPanel : public juce::Component
{
public:
    SpectrumPanel(te::Edit& edit, EditViewState& view);

    /** Takes the master analyzer's latest sound and moves the bars on to `seconds`, a frame's time; repaints
        if they moved. Once per display frame, or per video frame when a video is rendered off screen. */
    void refresh(double seconds);

    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int gap = 8;  // on the side towards the notes

private:
    te::Edit& edit_;
    EditViewState& view_;
    const bool atLeft_;  // else along the bottom
    SpectrumBars bars_;
    std::array<float, SpectrumBars::fftSize> samples_{};
    std::optional<double> lastSeconds_;

    juce::ComponentBoundsConstrainer constrainer_;
    juce::ResizableEdgeComponent resizer_{this,
                                          &constrainer_,
                                          atLeft_ ? juce::ResizableEdgeComponent::rightEdge
                                                  : juce::ResizableEdgeComponent::topEdge};
    juce::VBlankAttachment vblank_{this, [this](double seconds) { refresh(seconds); }};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumPanel)
};

}  // namespace MoTool::uZX
