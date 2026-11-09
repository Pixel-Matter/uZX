#include "RememberedStereo.h"
#include "PropertyStorage.h"

#include "../plugins/uZX/aychip/AYPlugin.h"

namespace MoTool {

static PropertyStorage& getStorage(tracktion::Edit& edit) {
    return dynamic_cast<PropertyStorage&>(edit.engine.getPropertyStorage());
}

void RememberedStereo::apply(uZX::AYChipPlugin& chip) {
    auto& storage = getStorage(chip.edit);
    if (const auto layout = storage.getCustomProperty(layoutKey); layout.toString().isNotEmpty())
        chip.dynamicParams.layout.setStoredValue(VariantConverter<uZX::ChannelsLayout>::fromVar(layout));
    if (const auto width = storage.getCustomProperty(widthKey).toString(); width.isNotEmpty())
        chip.dynamicParams.stereoWidth.setStoredValue(width.getFloatValue());
}

void RememberedStereo::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop) {
    if ((prop == uZX::IDs::layout || prop == uZX::IDs::stereo)
        && tree[tracktion::IDs::type].toString() == uZX::AYChipPlugin::xmlTypeName)
        getStorage(edit_).setCustomProperty(prop == uZX::IDs::layout ? layoutKey : widthKey, tree[prop]);
}

}  // namespace MoTool
