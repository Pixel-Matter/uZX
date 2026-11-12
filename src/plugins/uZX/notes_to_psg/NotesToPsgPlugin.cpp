#include "NotesToPsgPlugin.h"
#include "../aychip/AYPlugin.h"
#include "../../../models/tuning/TuningRegistry.h"

namespace MoTool::uZX {


//==============================================================================
/** The clock of the first AY chip after the plugin in its list, read from the edit state, so a chip
    whose plugin object is not created yet counts as well. */
static double findChipClockAfter(const ValueTree& pluginState) {
    const auto list = pluginState.getParent();
    for (int i = list.indexOf(pluginState) + 1; i < list.getNumChildren(); ++i)
        if (const auto p = list.getChild(i);
            p.hasType(te::IDs::PLUGIN) && p[te::IDs::type].toString() == AYChipPlugin::xmlTypeName)
            return static_cast<double>(p.getProperty(IDs::clock, AYChipPlugin::defaultClockMhz)) * MHz;
    return AYChipPlugin::defaultClockMhz * MHz;
}

void NotesToPsgPlugin::ChipClockWatcher::watch(ValueTree newList) {
    list.removeListener(this);
    list = std::move(newList);
    list.addListener(this);
}

void NotesToPsgPlugin::ChipClockWatcher::valueTreePropertyChanged(ValueTree&, const Identifier& id) {
    if (id == IDs::clock)
        plugin.followChipClock();
}

void NotesToPsgPlugin::ChipClockWatcher::valueTreeChildAdded(ValueTree& parent, ValueTree&) {
    if (parent == list)
        plugin.followChipClock();
}

void NotesToPsgPlugin::ChipClockWatcher::valueTreeChildRemoved(ValueTree& parent, ValueTree&, int) {
    if (parent == list)
        plugin.followChipClock();
}

void NotesToPsgPlugin::ChipClockWatcher::valueTreeChildOrderChanged(ValueTree& parent, int, int) {
    if (parent == list)
        plugin.followChipClock();
}

//==============================================================================
const char* NotesToPsgPlugin::xmlTypeName = "uzxmidi2psg";

NotesToPsgPlugin::NotesToPsgPlugin(te::PluginCreationInfo info)
    : MidiFxPluginBase<NotesToPsgMapper>(info, transformer)
    , chipClock_(findChipClockAfter(state))
{
    staticParams.referTo(state, getUndoManager());
    chipClockWatcher.watch(state.getParent());

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

void NotesToPsgPlugin::valueTreeParentChanged(ValueTree& v) {
    MidiFxPluginBase<NotesToPsgMapper>::valueTreeParentChanged(v);
    chipClockWatcher.watch(state.getParent());
    followChipClock();
}

void NotesToPsgPlugin::followChipClock() {
    const auto clock = findChipClockAfter(state);
    if (exactlyEqual(clock, chipClock_))
        return;
    chipClock_ = clock;
    recreateTuningSystem();
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
    // Silences any hanging notes as well
    midiEffect.setBaseChannel(static_cast<int>(staticParams.baseMidiChannel.getStoredValue()));
}

void NotesToPsgPlugin::recreateTuningSystem() {
    // DBG("Recreating tuning system with index: " << tuningTableIndex0.get());
    auto builtinTable = staticParams.tuningTable.getStoredValue();
    // auto scaleType = staticParams.scaleType.getStoredValue();

    TuningOptions options {
        .builtinTable = builtinTable,
        // .scaleType = scaleType
    };

    std::shared_ptr<TuningSystem> tuning = makeBuiltinTuning(options);
    // A tuning by pitch is computed for the chip's clock; a tracker table's periods are the same on any clock
    if (dynamic_cast<AutoTuning*>(tuning.get()) != nullptr)
        tuning->setClockFrequency(chipClock_);
    setTuningSystem(std::move(tuning));
}

TuningSystem& NotesToPsgPlugin::getTuningSystem() const {
    return midiEffect.getTuningSystem();
}

void NotesToPsgPlugin::setTuningSystem(std::shared_ptr<TuningSystem> tuningSystem) {
    jassert(tuningSystem != nullptr);
    midiEffect.setTuningSystem(std::move(tuningSystem));
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
