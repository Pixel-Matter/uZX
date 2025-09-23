#include "NotesToPsgPlugin.h"

namespace MoTool::uZX {

const char* NotesToPsgPlugin::xmlTypeName = "uzxmidi2psg";

NotesToPsgPlugin::NotesToPsgPlugin(te::PluginCreationInfo info)
    : MidiFxPluginBase<NotesToPsgMapper>(info, transformer)
{
    midiEffect.setBaseChannel(1);
}

NotesToPsgPlugin::~NotesToPsgPlugin() {
}

void NotesToPsgPlugin::initialise(const te::PluginInitialisationInfo&) {
    updateParams();
}

void NotesToPsgPlugin::deinitialise() {
}

void NotesToPsgPlugin::midiPanic() {
    reset();
}

void NotesToPsgPlugin::reset() {
    midiEffect.clearOutput();
    midiEffect.reset();
}

void NotesToPsgPlugin::restorePluginStateFromValueTree(const ValueTree& v) {
    staticParams.restoreFromTree(v);
    updateParams();
}

std::unique_ptr<te::Plugin::EditorComponent> NotesToPsgPlugin::createEditor() {
    // Return nullptr for now - no GUI needed for basic functionality
    return nullptr;
}

void NotesToPsgPlugin::Params::initialise() {
    baseMidiChannelValue.referTo(IDs::midiBase, "Base MIDI channel", {1, 16, 1}, 1);
    // numChannelsValue.referTo(IDs::midiChans, "Number of channels", {1, 4, 1}, 4);
}

void NotesToPsgPlugin::Params::restoreFromTree(const juce::ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        baseMidiChannelValue.cachedValue
        // numChannelsValue.cachedValue
    );
}

void NotesToPsgPlugin::valueTreeChanged() {
    updateParams();
}

void NotesToPsgPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    juce::ignoreUnused(v);

    if (id == IDs::midiBase || id == IDs::midiChans) {
        updateParams();
    }
}

void NotesToPsgPlugin::updateParams() {
    // first reset to clear state and to silence any hanging notes
    reset();
    midiEffect.setBaseChannel(staticParams.baseMidiChannelValue.get());
    // midiEffect.setNumChannels(staticParams.numChannelsValue.get());
    // then reset again to init
    reset();
}

void NotesToPsgPlugin::setTuningSystem(TuningSystem* tuningSystem) {
    currentTuningSystem = tuningSystem;
    midiEffect.setTuningSystem(currentTuningSystem);
}

} // namespace MoTool::uZX