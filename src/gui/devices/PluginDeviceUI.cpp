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

    if (submenu.getNumItems() > 0) {
        juce::String label = title;
        if (label.isNotEmpty())
            label += " (" + juce::String(currentValue) + ")";
        else
            label = juce::String(currentValue);

        parentMenu.addSubMenu(label, submenu);
    }
}

void PluginDeviceUI::addMidiRangeMenu(juce::PopupMenu& parentMenu, ParameterValue<int>& midiParameter,
                                      const juce::String& title, int range)
{
    juce::PopupMenu submenu;

    const auto currentChan = midiParameter.getStoredValue();
    auto start = static_cast<int>(midiParameter.definition.valueRange.start);
    auto end = static_cast<int>(midiParameter.definition.valueRange.end);

    jassert(midiParameter.definition.valueRange.interval == 1);
    jassert(start <= end);

    for (int chan = start; chan <= end + range - 1; ++chan) {
        submenu.addItem(juce::String(chan),
                        chan <= end,
                        chan >= currentChan && chan <= currentChan + range - 1,
                        [&midiParameter, chan, end]() {
                            if (chan <= end)
                                midiParameter.setStoredValue(chan);
                        });
    }

    if (submenu.getNumItems() > 0) {
        String label = juce::String(currentChan) + " - " + juce::String(juce::jmin(currentChan + range - 1, 16));
        if (title.isNotEmpty())
            label = title + " (" + label + ")";

        parentMenu.addSubMenu(label, submenu);
    }
}

}  // namespace MoTool
