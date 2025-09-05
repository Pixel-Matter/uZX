#include "PluginDeviceUI.h"
#include "LevelMeterUI.h"

namespace MoTool {

PluginDeviceUI::PluginDeviceUI(EditViewState& evs, tracktion::Plugin::Ptr p)
    : editViewState(evs), plugin(p)
{
}

PluginDeviceUI::~PluginDeviceUI()
{
}

void PluginDeviceUI::mouseDown(const juce::MouseEvent& e)
{
    pluginClicked(e.mods);
}

void PluginDeviceUI::pluginClicked(const juce::ModifierKeys& modifiers)
{
    if (plugin)
    {
        editViewState.selectionManager.selectOnly(plugin.get());
        
        if (modifiers.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem("Delete", [this] { plugin->deleteFromParent(); });
            m.showAt(this);
        }
        else
        {
            plugin->showWindowExplicitly();
        }
    }
}

std::unique_ptr<PluginDeviceUI> PluginDeviceUI::createForPlugin(EditViewState& evs, tracktion::Plugin::Ptr plugin)
{
    if (!plugin)
        return nullptr;
        
    // Check for specific plugin types that need custom UIs
    if (dynamic_cast<tracktion::LevelMeterPlugin*>(plugin.get()))
    {
        return std::make_unique<LevelMeterUI>(evs, plugin);
    }
    
    // Default: no custom UI
    return nullptr;
}

bool PluginDeviceUI::hasCustomDeviceUI(tracktion::Plugin::Ptr plugin)
{
    if (!plugin)
        return false;
        
    // Check for plugins that have custom device UIs
    if (dynamic_cast<tracktion::LevelMeterPlugin*>(plugin.get()))
        return true;
        
    return false;
}

}  // namespace MoTool