#include "PluginDeviceUI.h"
#include "PluginUIAdapterRegistry.h"


namespace MoTool {

PluginDeviceUI::PluginDeviceUI(tracktion::Plugin::Ptr p)
    : plugin(p)
{}

PluginDeviceUI::~PluginDeviceUI() {}

}  // namespace MoTool