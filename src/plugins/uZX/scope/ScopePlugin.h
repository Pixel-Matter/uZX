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
    DECLARE_ID(sourceMode)
    DECLARE_ID(channelOffset)
    #undef DECLARE_ID
}  // namespace ScopeIDs


/**
 * Oscilloscope plugin for visualizing audio waveforms.
 *
 * Placed after AYPlugin in chain to display individual channel waveforms.
 *
 * Channel handling:
 * - 5-channel input (from AYPlugin): Displays channels 2,3,4 (A,B,C raw),
 *   passes through channels 0,1 (stereo mix), outputs 2 channels
 * - 2-channel input (stereo): Displays L+R mono sum, passes through unchanged
 * NOTE: Multichannel in Traction Engine is not currently supported
 *       and sidechain is for cross-track use only.
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

    void initialise(const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void getChannelNames(StringArray*, StringArray*) override;
    void applyToBuffer(const te::PluginRenderContext&) override;

    //==============================================================================
    bool takesMidiInput() override                      { return false; }
    bool takesAudioInput() override                     { return true; }
    bool producesAudioWhenNoAudioInput() override       { return false; }
    void restorePluginStateFromValueTree(const juce::ValueTree&) override;

    //==============================================================================
    // Parameter access

    static constexpr int kMaxDisplayChannels = 3;  // Max waveforms to display

    /**
     * Source mode selection.
     */
    struct SourceModeEnum {
        enum Enum {
            StereoMix = 0,       // Display channels 0-1 (L, R stereo mix)
            SeparateChannels = 1 // Display separate channels starting from offset
        };
    };
    using SourceMode = Util::EnumChoice<SourceModeEnum>;

    /**
     * Input mode detected from channel count.
     */
    enum class InputMode {
        Stereo,      // 2-channel input: display L and R separately
        AYSeparate   // 5-channel input: display A, B, C (channels 2,3,4)
    };

    /**
     * Get current number of display channels.
     * Returns 3 for AY mode, 2 for stereo mode.
     */
    int getNumDisplayChannels() const {
        return (inputMode_.load(std::memory_order_relaxed) == InputMode::AYSeparate) ? 3 : 2;
    }

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
            visitor(sourceMode);
            visitor(channelOffset);
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
             TriggerMode::RisingEdge}
        };

        ParameterValue<float> triggerLevel {
            {"trigLevel", ScopeIDs::triggerLevel, "Level", "Trigger threshold level",
             0.0f, {-1.0f, 1.0f, 0.01f}}
        };

        ParameterValue<SourceMode> sourceMode {
            {"source", ScopeIDs::sourceMode, "Source", "Channel source (0=Stereo Mix L/R, 1=Separate Channels)",
             SourceMode::StereoMix}
        };

        ParameterValue<int> channelOffset {
            {"offset", ScopeIDs::channelOffset, "Offset", "Starting channel for separate mode (default 2 = A,B,C)",
             2, {0, 4, 1}}
        };
    };

    StaticParams staticParams;

    //==============================================================================
    // Buffer access for UI

    /**
     * Get scope buffer for display channel (0-indexed).
     * In AY mode: returns A, B, C channel buffers
     * In Stereo mode: returns L, R channel buffers
     * Returns nullptr if channel index is out of range.
     */
    const ScopeBuffer* getBuffer(int channel) const {
        if (channel >= 0 && channel < kMaxDisplayChannels)
            return &buffers_[static_cast<size_t>(channel)];
        return nullptr;
    }

    /**
     * Get current input mode.
     */
    InputMode getInputMode() const {
        return inputMode_.load(std::memory_order_relaxed);
    }

    //==============================================================================
    // Sidechain management

    /**
     * Get all plugins on the same track as this plugin.
     */
    std::vector<te::Plugin::Ptr> getPluginsOnTrack() const;

    /**
     * Get the plugin immediately before this one on the track.
     */
    te::Plugin::Ptr getPreviousPluginOnTrack() const;

    /**
     * Auto-set sidechain source to the previous plugin on the track.
     * Returns true if successful.
     */
    bool autoSetSidechainToPreviousPlugin();

private:
    std::array<ScopeBuffer, kMaxDisplayChannels> buffers_;
    std::atomic<InputMode> inputMode_{InputMode::Stereo};

    void valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& id) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopePlugin)
};

}  // namespace MoTool::uZX


namespace juce {

using namespace MoTool::uZX;
using namespace MoTool::Util;

template <>
struct VariantConverter<ScopePlugin::SourceMode> : public EnumVariantConverter<ScopePlugin::SourceMode> {};

}