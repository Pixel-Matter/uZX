#include "PluginDeviceUI.h"
#include "PluginUIAdapterRegistry.h"

#include "LevelMeterUI.h"

namespace MoTool {

PluginDeviceUI::PluginDeviceUI(EditViewState& evs, tracktion::Plugin::Ptr p)
    // : editViewState(evs),
    : plugin(p)
{}

PluginDeviceUI::~PluginDeviceUI() {}

}  // namespace MoTool