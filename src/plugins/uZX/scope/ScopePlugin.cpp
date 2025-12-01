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
    const int numSamples = fc.bufferNumSamples;
    const int startSample = fc.bufferStartSample;

    // Display stereo (L, R) - channels 0, 1
    if (numInputChannels >= 2) {
        const float* left = fc.destBuffer->getReadPointer(0, startSample);
        const float* right = fc.destBuffer->getReadPointer(1, startSample);
        buffers_[0].pushSamples(left, numSamples);
        buffers_[1].pushSamples(right, numSamples);
    } else if (numInputChannels == 1) {
        // Mono input - display in both buffers
        const float* mono = fc.destBuffer->getReadPointer(0, startSample);
        buffers_[0].pushSamples(mono, numSamples);
        buffers_[1].pushSamples(mono, numSamples);
    }

    // Audio passes through unchanged
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

}  // namespace MoTool::uZX
