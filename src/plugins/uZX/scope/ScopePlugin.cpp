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

    const int numInputChannels = fc.destBuffer->getNumChannels();
    const int numSamples = fc.bufferNumSamples;
    const int startSample = fc.bufferStartSample;

    // Determine input mode based on channel count
    const InputMode mode = (numInputChannels == 5) ? InputMode::AYSeparate : InputMode::Stereo;
    inputMode_.store(mode, std::memory_order_relaxed);

    if (mode == InputMode::AYSeparate) {
        // 5-channel mode from AYPlugin:
        // Input: L(0), R(1), A(2), B(3), C(4)
        // Display channels 2,3,4 (A,B,C raw)
        for (int displayCh = 0; displayCh < 3; ++displayCh) {
            const int inputCh = displayCh + 2;  // Map display 0,1,2 → input 2,3,4
            const float* channelData = fc.destBuffer->getReadPointer(inputCh, startSample);
            buffers_[static_cast<size_t>(displayCh)].pushSamples(channelData, numSamples);
        }
        
        // Channels 0,1 (stereo) pass through unchanged.
        // The engine will only use first 2 channels based on getNumOutputChannelsGivenInputs(5) → 2
        
    } else {
        // Stereo mode: Display L and R separately
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
        
        // Audio passes through unchanged for stereo mode
    }
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
