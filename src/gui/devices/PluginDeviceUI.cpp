#include "PluginDeviceUI.h"
#include "PluginUIAdapterRegistry.h"
#include "../../controllers/Parameters.h"


namespace MoTool {

PluginDeviceUI::PluginDeviceUI(tracktion::Plugin::Ptr p)
    : plugin(p)
{}

PluginDeviceUI::~PluginDeviceUI() {}

void PluginDeviceUI::addDiscreteIntegerParameterMenu(juce::PopupMenu& parentMenu,
                                                     ParameterValue<int>& parameter,
                                                     const juce::String& title)
{
    juce::PopupMenu submenu;

    const auto currentValue = parameter.getStoredValue();
    auto start = static_cast<int>(parameter.definition.valueRange.start);
    auto end = static_cast<int>(parameter.definition.valueRange.end);
    auto step = static_cast<int>(parameter.definition.valueRange.interval);

    const bool iterateForward = start <= end;
    if (step == 0)
        step = iterateForward ? 1 : -1;
    if (iterateForward && step < 0)
        step = -step;
    if (!iterateForward && step > 0)
        step = -step;

    for (int value = start;
         iterateForward ? value <= end : value >= end;
         value += step)
    {
        submenu.addItem(juce::String(value),
                        true,
                        value == currentValue,
                        [&parameter, value]() { parameter.setStoredValue(value); });
    }

    if (submenu.getNumItems() > 0)
        parentMenu.addSubMenu(title, submenu);
}

}  // namespace MoTool
