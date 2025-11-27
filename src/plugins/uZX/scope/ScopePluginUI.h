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

namespace MoTool::uZX {

//==============================================================================
/**
 * Single channel waveform display component.
 */
class WaveformDisplay : public juce::Component,
                        private juce::Timer {
public:
    WaveformDisplay(juce::Colour colour, const juce::String& label);
    ~WaveformDisplay() override;

    /**
     * Set the source buffer for this display (non-owning pointer).
     */
    void setBuffer(const ScopeBuffer* buffer);

    /**
     * Update display parameters.
     */
    void setWindowSize(int samples);
    void setGain(float gain);
    void setTriggerStrategy(const TriggerStrategy& strategy);
    void setTriggerLevel(float level);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateWaveform();
    int findTriggerPoint();
    void buildPath();

    const ScopeBuffer* sourceBuffer_{nullptr};
    std::vector<float> displaySamples_;
    std::vector<float> workBuffer_;  // Larger buffer for trigger search
    juce::Path waveformPath_;

    juce::Colour colour_;
    juce::String label_;

    int windowSize_{1024};
    float gain_{1.0f};
    TriggerStrategy triggerStrategy_;
    float triggerLevel_{0.0f};

    static constexpr int kRefreshRateHz = 30;
    static constexpr int kTriggerSearchMultiplier = 2;  // Search in 2x window for trigger

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
    static constexpr int kControlsHeight = 28;
    static constexpr int kSpacing = 4;

private:
    void valueChanged(juce::Value& value) override;
    void updateDisplayParams();
    void updateDisplayCount();

    ScopePlugin& plugin_;
    juce::ComponentBoundsConstrainer constrainer_;

    // Control widgets
    ChoiceButton triggerButton_;
    LabeledSlider windowSlider_;
    LabeledSlider gainSlider_;
    LabeledSlider levelSlider_;

    // Channel displays - dynamically shown based on input mode
    std::array<std::unique_ptr<WaveformDisplay>, ScopePlugin::kMaxDisplayChannels> displays_;
    ScopePlugin::InputMode currentInputMode_{ScopePlugin::InputMode::Stereo};
    int visibleDisplayCount_{0};

    // Channel colors from PSG palette
    inline static const std::array<juce::Colour, 3> channelColours_{
        Colors::PSG::A,  // Blue
        Colors::PSG::B,  // Emerald
        Colors::PSG::C   // Amber
    };

    // Labels for AY 3-channel mode (A, B, C)
    static constexpr std::array<const char*, 3> ayChannelLabels_{
        "A", "B", "C"
    };

    // Labels for stereo mode (L, R)
    static constexpr std::array<const char*, 2> stereoLabels_{
        "L", "R"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopePluginUI)
};

}  // namespace MoTool::uZX
