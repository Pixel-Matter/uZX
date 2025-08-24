#include "NotesToPsgPlugin.h"

namespace MoTool::uZX {

const char* NotesToPsgPlugin::xmlTypeName = "uzxmidi2psg";

NotesToPsgPlugin::NotesToPsgPlugin(te::PluginCreationInfo info)
    : MidiFxPluginBase<NotesToPsgMapper>(info)
{
    midiEffect.setBaseChannel(1);
    midiEffect.setNumChannels(4);
}

NotesToPsgPlugin::~NotesToPsgPlugin() {
}

void NotesToPsgPlugin::initialise(const te::PluginInitialisationInfo&) {
    updateConverterParams();
}

void NotesToPsgPlugin::deinitialise() {
}

void NotesToPsgPlugin::midiPanic() {
    reset();
}

void NotesToPsgPlugin::reset() {
    midiEffect.clearOutput();
    midiEffect.initPSG();
}

void NotesToPsgPlugin::restorePluginStateFromValueTree(const ValueTree& v) {
    staticParams.restoreFromTree(v);
    updateConverterParams();
}

std::unique_ptr<te::Plugin::EditorComponent> NotesToPsgPlugin::createEditor() {
    // Return nullptr for now - no GUI needed for basic functionality
    return nullptr;
}

void NotesToPsgPlugin::Params::initialise() {
    baseMidiChannelValue.referTo(IDs::midiBase, "Base MIDI channel", {1, 16, 1}, 1);
    numChannelsValue.referTo(IDs::midiChans, "Number of channels", {1, 4, 1}, 4);
}

void NotesToPsgPlugin::Params::restoreFromTree(const juce::ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        baseMidiChannelValue.cachedValue,
        numChannelsValue.cachedValue
    );
}

void NotesToPsgPlugin::valueTreeChanged() {
    updateConverterParams();
}

void NotesToPsgPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    juce::ignoreUnused(v);

    if (id == IDs::midiBase || id == IDs::midiChans) {
        updateConverterParams();
    }
}

void NotesToPsgPlugin::updateConverterParams() {
    midiEffect.setBaseChannel(staticParams.baseMidiChannelValue.get());
    midiEffect.setNumChannels(staticParams.numChannelsValue.get());
    reset();
}

void NotesToPsgPlugin::setTuningSystem(TuningSystem* tuningSystem) {
    currentTuningSystem = tuningSystem;
    midiEffect.setTuningSystem(currentTuningSystem);
}

} // namespace MoTool::uZX