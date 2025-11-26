#include "ScopePlugin.h"

namespace MoTool::uZX {

//==============================================================================
const char* ScopePlugin::xmlTypeName = "scope";

ScopePlugin::ScopePlugin(te::PluginCreationInfo info)
    : Plugin(info)
{
    staticParams.referTo(state, getUndoManager());
}

ScopePlugin::~ScopePlugin() {
    notifyListenersOfDeletion();
}

void ScopePlugin::initialise(const te::PluginInitialisationInfo&) {
    // Clear buffers on init
    for (auto& buffer : buffers_)
        buffer.clear();
}

void ScopePlugin::deinitialise() {
    // Nothing to clean up
}

void ScopePlugin::applyToBuffer(const te::PluginRenderContext& fc) {
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    const int numChannels = fc.destBuffer->getNumChannels();
    const int numSamples = fc.bufferNumSamples;

    // Update active channel count for UI
    numActiveChannels_.store(std::min(numChannels, kMaxChannels), std::memory_order_relaxed);

    // Push samples to each channel buffer
    for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch) {
        const float* channelData = fc.destBuffer->getReadPointer(ch, fc.bufferStartSample);
        buffers_[static_cast<size_t>(ch)].pushSamples(channelData, numSamples);
    }

    // Audio passes through unchanged - this is a visualization-only plugin
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
    staticParams.restoreStateFromValueTree(v);
}

}  // namespace MoTool::uZX
