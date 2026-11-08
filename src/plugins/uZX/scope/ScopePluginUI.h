#pragma once

#include <JuceHeader.h>

#include "ScopePlugin.h"
#include "ScopeBuffer.h"
#include "TriggerStrategy.h"
#include "../../../gui/common/ChoiceButton.h"
#include "../../../gui/common/LabeledSlider.h"
#include "../../../gui/devices/PluginDeviceUI.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"
#include "../../../gui/common/LookAndFeel.h"

#include <vector>
#include <array>
#include <utility>

namespace MoTool::uZX {

/**
 * Reduces samples to one vertical span per pixel column, so a peak between columns is never lost
 * as it would be by skipping samples. Each column takes an equal slice plus the last sample of
 * the slice before, so the spans join up; a span runs from min to max on a rise, max to min on a fall.
 */
std::vector<std::pair<float, float>> minMaxColumns(const float* samples, int numSamples, int columns);

//==============================================================================
/**
 * Single channel waveform display component. Updates once per display frame, in step with the
 * timeline, and repaints only when the triggered window or the display settings changed.
 */
class WaveformDisplay : public juce::Component {
public:
    WaveformDisplay(juce::Colour colour,
                    const juce::String& label,
                    const ScopeBuffer& buffer,
                    ScopeSettings& settings);

    /**
     * Update display parameters.
     */
    void setWindowSize(int samples);
    void setGain(float gain);
    void setTriggerStrategy(const TriggerStrategy& strategy);
    void setTriggerLevel(float level);
    /** The trace, and its label, in one colour on another: Viz mode's Classic colours. */
    void setColours(juce::Colour trace, juce::Colour background);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    /** Takes the latest window from the buffer and repaints if it changed: once per display frame,
        or per video frame when a video is rendered off screen. */
    void refresh();

private:
    /** Takes the triggered window from the buffer; false if it is the one already shown. */
    bool updateWaveform();
    int findTriggerPoint();
    void buildPath();
    void showPopupMenu();

    /** The y of a zero sample, and pixels per unit: at gain 1 the signal's range spans 90% of the height. */
    struct YAxis { float zero, scale; };
    YAxis yAxis(float height) const;

    const ScopeBuffer& sourceBuffer_;
    ScopeSettings& scopeSettings_;
    std::vector<float> displaySamples_;
    std::vector<float> workBuffer_;  // Larger buffer for trigger search
    std::vector<float> syncWork_;       // The work buffer's sync, searched instead in Chip Sync mode
    std::vector<float> amplitudeWork_;  // The work buffer's amplitude, which centres a chip channel
    float centre_{0.0f};                // Half the shown window's highest amplitude: a chip channel's middle
    juce::Path waveformPath_;

    juce::Colour colour_, background_;
    juce::String label_;

    int windowSize_{1024};
    float gain_{1.0f};
    TriggerStrategy triggerStrategy_;
    float triggerLevel_{0.0f};
    bool settingsChanged_{true};

    static constexpr int kTriggerSearchMultiplier = 2;  // Search in 2x window for trigger

    juce::VBlankAttachment vblank_{this, [this] { refresh(); }};  // last: detaches before the rest goes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};


//==============================================================================
/**
 * Plugin UI for ScopePlugin - shows multiple waveform displays.
 */
class ScopePluginUI : public PluginDeviceUI,
                      private juce::Value::Listener {
public:
    explicit ScopePluginUI(tracktion::Plugin::Ptr pluginPtr);
    ~ScopePluginUI() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::ComponentBoundsConstrainer* getBoundsConstrainer();

    bool hasDeviceMenu() const override;
    void populateDeviceMenu(juce::PopupMenu& menu) override;

    static constexpr int kChannelHeight = 80;
    static constexpr int kControlsHeight = 40;  // Consistent with AY plugin
    static constexpr int kSpacing = 4;

private:
    void valueChanged(juce::Value& value) override;
    void updateDisplayParams();

    ScopePlugin& plugin_;
    juce::ComponentBoundsConstrainer constrainer_;

    // Control widgets
    ChoiceButton triggerButton_;
    LabeledSlider windowSlider_;
    LabeledSlider gainSlider_;
    LabeledSlider levelSlider_;

    // Channel displays - stereo L and R
    std::array<std::unique_ptr<WaveformDisplay>, ScopePlugin::kMaxDisplayChannels> displays_;

    // Channel colors (L = blue, R = emerald)
    inline static const std::array<juce::Colour, 2> channelColours_{
        Colors::PSG::A,  // Blue (L)
        Colors::PSG::B   // Emerald (R)
    };

    // Labels for stereo mode (L, R)
    static constexpr std::array<const char*, 2> stereoLabels_{
        "L", "R"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopePluginUI)
};

}  // namespace MoTool::uZX
