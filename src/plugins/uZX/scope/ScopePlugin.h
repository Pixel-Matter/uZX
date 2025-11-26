#pragma once

#include <JuceHeader.h>

#include "../../../controllers/Parameters.h"
#include "ScopeBuffer.h"
#include "TriggerStrategy.h"

#include <array>
#include <atomic>

namespace te = tracktion;

namespace MoTool::uZX {

namespace ScopeIDs {
    #define DECLARE_ID(name) inline const juce::Identifier name(#name);
    DECLARE_ID(windowSamples)
    DECLARE_ID(gain)
    DECLARE_ID(triggerMode)
    DECLARE_ID(triggerLevel)
    #undef DECLARE_ID
}  // namespace ScopeIDs


/**
 * Oscilloscope plugin for visualizing audio waveforms.
 * 
 * Placed after AYPlugin in chain to display individual channel waveforms.
 * Audio passes through unchanged - this is purely a visualization plugin.
 * 
 * Features:
 * - Per-channel waveform display (auto-detects channel count)
 * - Configurable trigger modes (free-running, rising/falling edge)
 * - Adjustable time window and display gain
 */
class ScopePlugin : public te::Plugin {
public:
    ScopePlugin(te::PluginCreationInfo);
    ~ScopePlugin() override;

    //==============================================================================
    static const char* getPluginName()              { return "Scope"; }
    static const char* xmlTypeName;

    juce::String getVendor() override               { return "PixelMatter"; }
    juce::String getName() const override           { return juce::String::fromUTF8(getPluginName()); }
    juce::String getPluginType() override           { return xmlTypeName; }
    juce::String getShortName(int) override         { return "Scope"; }
    juce::String getSelectableDescription() override { return "Oscilloscope display"; }
    bool isSynth() override                         { return false; }

    int getNumOutputChannelsGivenInputs(int numInputChannels) override { return numInputChannels; }
    void initialise(const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer(const te::PluginRenderContext&) override;

    //==============================================================================
    bool takesMidiInput() override                      { return false; }
    bool takesAudioInput() override                     { return true; }
    bool producesAudioWhenNoAudioInput() override       { return false; }
    void restorePluginStateFromValueTree(const juce::ValueTree&) override;

    //==============================================================================
    // Parameter access
    
    static constexpr int kMaxChannels = 3;
    
    /**
     * Static parameters (not automated, changed via UI).
     */
    struct StaticParams : ParamsBase<StaticParams> {
        using ParamsBase<StaticParams>::ParamsBase;

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(windowSamples);
            visitor(gain);
            visitor(triggerMode);
            visitor(triggerLevel);
        }

        ParameterValue<int> windowSamples {
            {"window", ScopeIDs::windowSamples, "Window", "Display window size (samples)",
             1024, {256, 4096, 256}}
        };
        
        ParameterValue<float> gain {
            {"gain", ScopeIDs::gain, "Gain", "Display gain multiplier",
             1.0f, {0.1f, 10.0f, 0.1f}}
        };
        
        ParameterValue<TriggerMode> triggerMode {
            {"trigger", ScopeIDs::triggerMode, "Trigger", "Trigger mode",
             TriggerMode(TriggerModeEnum::RisingEdge)}
        };
        
        ParameterValue<float> triggerLevel {
            {"trigLevel", ScopeIDs::triggerLevel, "Level", "Trigger threshold level",
             0.0f, {-1.0f, 1.0f, 0.01f}}
        };
    };

    StaticParams staticParams;

    //==============================================================================
    // Buffer access for UI
    
    /**
     * Get scope buffer for channel (0-indexed).
     * Returns nullptr if channel index is out of range.
     */
    const ScopeBuffer* getBuffer(int channel) const {
        if (channel >= 0 && channel < kMaxChannels)
            return &buffers_[static_cast<size_t>(channel)];
        return nullptr;
    }
    
    /**
     * Get current number of active channels (from last processed buffer).
     */
    int getNumActiveChannels() const {
        return numActiveChannels_.load(std::memory_order_relaxed);
    }

private:
    std::array<ScopeBuffer, kMaxChannels> buffers_;
    std::atomic<int> numActiveChannels_{0};

    void valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& id) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopePlugin)
};

}  // namespace MoTool::uZX
