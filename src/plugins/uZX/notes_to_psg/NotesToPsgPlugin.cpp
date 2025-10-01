#include "NotesToPsgPlugin.h"

namespace MoTool::uZX {


//==============================================================================
const char* NotesToPsgPlugin::xmlTypeName = "uzxmidi2psg";

NotesToPsgPlugin::NotesToPsgPlugin(te::PluginCreationInfo info)
    : MidiFxPluginBase<NotesToPsgMapper>(info, transformer)
{
    staticParams.referTo(state, getUndoManager());

    midiEffect.setBaseChannel(static_cast<int>(staticParams.baseMidiChannel.getStoredValue()));
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
    staticParams.restoreStateFromValueTree(v);

    updateParams();
}

std::unique_ptr<te::Plugin::EditorComponent> NotesToPsgPlugin::createEditor() {
    return nullptr;
    // return std::make_unique<NotesToPsgPluginEditor>(NotesToPsgPlugin::Ptr(this));
}

void NotesToPsgPlugin::valueTreeChanged() {
    DBG("NotesToPsgPlugin::valueTreeChanged");
    updateParams();
}

void NotesToPsgPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    juce::ignoreUnused(v);

    if (id == IDs::midiBase || id == IDs::midiChans) {
        staticParams.baseMidiChannel.value.forceUpdateOfCachedValue();
        updateParams();
    }
}

void NotesToPsgPlugin::updateParams() {
    // first reset to clear state and to silence any hanging notes
    reset();
    midiEffect.setBaseChannel(static_cast<int>(staticParams.baseMidiChannel.getStoredValue()));
    // then reset again to init
    reset();
}

void NotesToPsgPlugin::setTuningSystem(TuningSystem* tuningSystem) {
    currentTuningSystem = tuningSystem;
    midiEffect.setTuningSystem(currentTuningSystem);
}

} // namespace MoTool::uZX
