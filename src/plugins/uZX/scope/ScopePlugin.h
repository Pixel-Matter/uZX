#pragma once

#include <JuceHeader.h>

#include "ScopeBuffer.h"
#include "ScopeSettings.h"

#include <array>
#include <atomic>

namespace te = tracktion;

namespace MoTool::uZX {


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

    static constexpr int kMaxDisplayChannels = 2;  // Stereo L and R

    ScopeSettings scopeSettings;

    //==============================================================================
    // Buffer access for UI

    /**
     * Get scope buffer for display channel (0-indexed).
     * Returns L or R channel buffer.
     * Returns nullptr if channel index is out of range.
     */
    const ScopeBuffer* getBuffer(int channel) const {
        if (channel >= 0 && channel < kMaxDisplayChannels)
            return &buffers_[static_cast<size_t>(channel)];
        return nullptr;
    }

private:
    std::array<ScopeBuffer, kMaxDisplayChannels> buffers_;

    void valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& id) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopePlugin)
};

}  // namespace MoTool::uZX