#include "NotesToPsgPlugin.h"
#include "../aychip/AYPlugin.h"
#include "../../../models/tuning/TuningRegistry.h"

namespace MoTool::uZX {


//==============================================================================
/** The first AY chip after the plugin in its list, as its state in the edit, so a chip whose plugin
    object is not created yet counts as well. Without one, a state that has nothing set. */
static ValueTree findChipAfter(const ValueTree& pluginState) {
    const auto list = pluginState.getParent();
    for (int i = list.indexOf(pluginState) + 1; i < list.getNumChildren(); ++i)
        if (const auto p = list.getChild(i);
            p.hasType(te::IDs::PLUGIN) && p[te::IDs::type].toString() == AYChipPlugin::xmlTypeName)
            return p;
    return {};
}

static double clockOf(const ValueTree& chip) {
    return static_cast<double>(chip.getProperty(IDs::clock, AYChipPlugin::defaultClockMhz)) * MHz;
}

// ponytail: the layout as stored; an automated layout is not followed until something needs it
static std::array<double, 3> channelPansOf(const ValueTree& chip) {
    return channelPans(chip.hasProperty(IDs::layout) ? VariantConverter<ChannelsLayout>::fromVar(chip[IDs::layout])
                                                     : ChannelsLayout(AYChipPlugin::defaultLayout));
}

void NotesToPsgPlugin::ChipWatcher::watch(ValueTree newList) {
    list.removeListener(this);
    list = std::move(newList);
    list.addListener(this);
}

void NotesToPsgPlugin::ChipWatcher::valueTreePropertyChanged(ValueTree&, const Identifier& id) {
    if (id == IDs::clock || id == IDs::layout)
        plugin.followChip();
}

void NotesToPsgPlugin::ChipWatcher::valueTreeChildAdded(ValueTree& parent, ValueTree&) {
    if (parent == list)
        plugin.followChip();
}

void NotesToPsgPlugin::ChipWatcher::valueTreeChildRemoved(ValueTree& parent, ValueTree&, int) {
    if (parent == list)
        plugin.followChip();
}

void NotesToPsgPlugin::ChipWatcher::valueTreeChildOrderChanged(ValueTree& parent, int, int) {
    if (parent == list)
        plugin.followChip();
}

//==============================================================================
const char* NotesToPsgPlugin::xmlTypeName = "uzxmidi2psg";

NotesToPsgPlugin::NotesToPsgPlugin(te::PluginCreationInfo info)
    : MidiFxPluginBase<NotesToPsgMapper>(info, transformer)
    , chipClock_(clockOf(findChipAfter(state)))
{
    staticParams.referTo(state, getUndoManager());
    chipWatcher.watch(state.getParent());

    midiEffect.setBaseChannel(static_cast<int>(staticParams.baseMidiChannel.getStoredValue()));
    midiEffect.setChannelPans(channelPansOf(findChipAfter(state)));
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
    chipWatcher.watch(state.getParent());
    followChip();
}

void NotesToPsgPlugin::followChip() {
    const auto chip = findChipAfter(state);
    midiEffect.setChannelPans(channelPansOf(chip));
    const auto clock = clockOf(chip);
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
