#include "NotesToPsgPlugin.h"
#include "../aychip/AYPlugin.h"
#include "../../../models/tuning/TuningRegistry.h"

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
    recreateTuningSystem();
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
    recreateTuningSystem();

    // IMPOTANT! To restore automated parameters properly
    PluginBase::restorePluginStateFromValueTree(v);
}

std::unique_ptr<te::Plugin::EditorComponent> NotesToPsgPlugin::createEditor() {
    return nullptr;
    // return std::make_unique<NotesToPsgPluginEditor>(NotesToPsgPlugin::Ptr(this));
}

void NotesToPsgPlugin::valueTreeChanged() {
    updateParams();
}

void NotesToPsgPlugin::valueTreePropertyChanged(ValueTree& v, const Identifier& id) {
    ignoreUnused(v);

    if (id == IDs::midiBase || id == IDs::midiChans) {
        staticParams.baseMidiChannel.forceUpdateOfCachedValue();
        updateParams();
    } else if (id == IDs::tuningTable) {
        staticParams.tuningTable.forceUpdateOfCachedValue();
        recreateTuningSystem();
    }
    // else if (id == IDs::scaleType) {
    //     staticParams.scaleType.forceUpdateOfCachedValue();
    //     recreateTuningSystem();
    // }
}

void NotesToPsgPlugin::updateParams() {
    // first reset to clear state and to silence any hanging notes
    reset();
    midiEffect.setBaseChannel(static_cast<int>(staticParams.baseMidiChannel.getStoredValue()));
    // then reset again to init
    reset();
}

void NotesToPsgPlugin::recreateTuningSystem() {
    // DBG("Recreating tuning system with index: " << tuningTableIndex0.get());
    auto builtinTable = staticParams.tuningTable.getStoredValue();
    // auto scaleType = staticParams.scaleType.getStoredValue();

    TuningOptions options {
        .builtinTable = builtinTable,
        // .scaleType = scaleType
    };

    setTuningSystem(makeBuiltinTuning(options));
}

TuningSystem& NotesToPsgPlugin::getTuningSystem() const {
    return midiEffect.getTuningSystem();
}

void NotesToPsgPlugin::setTuningSystem(std::shared_ptr<TuningSystem> tuningSystem) {
    jassert(tuningSystem != nullptr);
    midiEffect.setTuningSystem(std::move(tuningSystem));
    auto& tuning = midiEffect.getTuningSystem();

    // TODO maybe invert dependency?
    if (auto* ayPlugin = findPluginAfter<AYChipPlugin>()) {
        ayPlugin->staticParams.chipClock.setStoredValue(tuning.getClockFrequency() / MHz);
    }
    sendChangeMessage();
}

// Scale::ScaleType NotesToPsgPlugin::getScaleType() const {
//     return staticParams.scaleType.getStoredValue();
// }

// void NotesToPsgPlugin::setScaleType(Scale::ScaleType scaleType) {
//     staticParams.scaleType.setStoredValue(scaleType);
//     recreateTuningSystem();
// }

} // namespace MoTool::uZX
