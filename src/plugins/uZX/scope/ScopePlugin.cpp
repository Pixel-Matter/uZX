#include "ScopePlugin.h"

namespace MoTool::uZX {

//==============================================================================
const char* ScopePlugin::xmlTypeName = "scope";

ScopePlugin::ScopePlugin(te::PluginCreationInfo info)
    : Plugin(info)
{
    scopeSettings.referTo(state, getUndoManager());
}

ScopePlugin::~ScopePlugin() {
    notifyListenersOfDeletion();
}

void ScopePlugin::initialise(const te::PluginInitialisationInfo&) {
    // Clear buffers on init
    for (auto& buffer : buffers_)
        buffer.clear();

    // Auto-set sidechain to previous plugin if not already set
    if (!getSidechainSourceID().isValid()) {
        autoSetSidechainToPreviousPlugin();
    }
}

void ScopePlugin::deinitialise() {
    // Nothing to clean up
}

void ScopePlugin::getChannelNames(StringArray* ins, StringArray* outs) {
    Plugin::getChannelNames(ins, outs);
    // if (ins != nullptr) {
    //     ins->add("Channel A");
    //     ins->add("Channel B");
    //     ins->add("Channel C");
    // }
}

void ScopePlugin::applyToBuffer(const te::PluginRenderContext& fc) {
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    const int numInputChannels = fc.destBuffer->getNumChannels();
    // DBG("ScopePlugin::applyToBuffer: numInputChannels=" << numInputChannels);
    const int numSamples = fc.bufferNumSamples;
    const int startSample = fc.bufferStartSample;

    // Read parameters
    const int sourceModeInt = scopeSettings.sourceMode.getStoredValue();
    const SourceMode source = static_cast<SourceMode>(sourceModeInt);
    const int channelOffset = scopeSettings.channelOffset.getStoredValue();

    // Determine input mode based on channel count
    const InputMode mode = (numInputChannels == 5) ? InputMode::AYSeparate : InputMode::Stereo;
    inputMode_.store(mode, std::memory_order_relaxed);

    if (source == SourceMode::StereoMix) {
        // Display stereo mix (L, R) - channels 0, 1
        if (numInputChannels >= 2) {
            const float* left = fc.destBuffer->getReadPointer(0, startSample);
            const float* right = fc.destBuffer->getReadPointer(1, startSample);
            buffers_[0].pushSamples(left, numSamples);
            buffers_[1].pushSamples(right, numSamples);
        } else if (numInputChannels == 1) {
            // Mono input - display in buffer 0 only
            const float* mono = fc.destBuffer->getReadPointer(0, startSample);
            buffers_[0].pushSamples(mono, numSamples);
        }
    } else {
        // Display separate channels starting from offset
        // For 5-channel input with offset=2: displays A(2), B(3), C(4)
        const int numDisplayChannels = std::min(kMaxDisplayChannels, numInputChannels - channelOffset);

        for (int displayCh = 0; displayCh < numDisplayChannels; ++displayCh) {
            const int inputCh = displayCh + channelOffset;
            if (inputCh < numInputChannels) {
                const float* channelData = fc.destBuffer->getReadPointer(inputCh, startSample);
                buffers_[static_cast<size_t>(displayCh)].pushSamples(channelData, numSamples);
            }
        }
    }

    // Audio passes through unchanged
    // For 5-channel input, only first 2 channels are output (L, R stereo)
    // based on getNumOutputChannelsGivenInputs(5) → 2
}

void ScopePlugin::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& id) {
    if (v == state) {
        // Handle any property changes if needed
        // Currently all static params are read directly by UI
        propertiesChanged();
    }
    Plugin::valueTreePropertyChanged(v, id);
}

void ScopePlugin::restorePluginStateFromValueTree(const juce::ValueTree& v) {
    scopeSettings.restoreStateFromValueTree(v);
}

//==============================================================================
// Sidechain management

std::vector<te::Plugin::Ptr> ScopePlugin::getPluginsOnTrack() const {
    std::vector<te::Plugin::Ptr> plugins;

    auto* track = getOwnerTrack();
    if (track == nullptr)
        return plugins;

    // Get all plugins on this track
    for (auto* plugin : track->pluginList) {
        if (plugin != nullptr)
            plugins.push_back(plugin);
    }

    return plugins;
}

te::Plugin::Ptr ScopePlugin::getPreviousPluginOnTrack() const {
    auto* track = getOwnerTrack();
    if (track == nullptr)
        return nullptr;

    te::Plugin::Ptr previousPlugin = nullptr;

    // Find this plugin in the list and return the one before it
    for (auto* plugin : track->pluginList) {
        if (plugin == this)
            return previousPlugin;  // Return the plugin we saw before this one
        previousPlugin = plugin;
    }

    return nullptr;
}

bool ScopePlugin::autoSetSidechainToPreviousPlugin() {
    auto prevPlugin = getPreviousPluginOnTrack();
    if (prevPlugin == nullptr) {
        DBG("ScopePlugin::autoSetSidechainToPreviousPlugin: No previous plugin found");
        return false;
    }

    setSidechainSourceID(prevPlugin->itemID);
    DBG("ScopePlugin: Set sidechain source to " << prevPlugin->getName() << " (ID: " << prevPlugin->itemID.toString() << ")");
    return true;
}

}  // namespace MoTool::uZX
