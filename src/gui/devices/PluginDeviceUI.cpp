#include "PluginDeviceUI.h"
#include "PluginUIAdapterRegistry.h"
#include "../../controllers/Parameters.h"
#include "../../plugins/uZX/scope/ScopeSettings.h"
#include "../../plugins/uZX/scope/TriggerStrategy.h"


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

void addScopeSettingsMenu(juce::PopupMenu& parentMenu,
                         uZX::ScopeSettings& settings,
                         const juce::String& title)
{
    using namespace uZX;

    // Determine target menu - use parent directly or create submenu
    juce::PopupMenu* targetMenu = &parentMenu;
    juce::PopupMenu scopeSubmenu;
    if (title.isNotEmpty()) {
        targetMenu = &scopeSubmenu;
    }

    // Window size presets
    juce::PopupMenu windowMenu;
    const int currentWindow = settings.windowSamples.getStoredValue();
    const std::array<int, 5> windowPresets = {256, 512, 1024, 2048, 4096};
    for (int preset : windowPresets) {
        windowMenu.addItem(juce::String(preset) + " samples",
            true,
            preset == currentWindow,
            [&settings, preset]() {
                settings.windowSamples.setStoredValue(preset);
            });
    }
    targetMenu->addSubMenu("Window Size (" + juce::String(currentWindow) + ")", windowMenu);

    // Gain presets
    juce::PopupMenu gainMenu;
    const float currentGain = settings.gain.getStoredValue();
    const std::array<float, 5> gainPresets = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    for (float preset : gainPresets) {
        gainMenu.addItem(juce::String(preset, 1) + "x",
            true,
            std::abs(preset - currentGain) < 0.01f,
            [&settings, preset]() {
                settings.gain.setStoredValue(preset);
            });
    }
    targetMenu->addSubMenu("Gain (" + juce::String(currentGain, 1) + "x)", gainMenu);

    // Trigger mode
    juce::PopupMenu triggerMenu;
    const auto currentMode = settings.triggerMode.getStoredValue();
    triggerMenu.addItem("Free Running",
        true,
        currentMode == TriggerMode::FreeRunning,
        [&settings]() {
            settings.triggerMode.setStoredValue(TriggerMode::FreeRunning);
        });
    triggerMenu.addItem("Rising Edge",
        true,
        currentMode == TriggerMode::RisingEdge,
        [&settings]() {
            settings.triggerMode.setStoredValue(TriggerMode::RisingEdge);
        });
    triggerMenu.addItem("Falling Edge",
        true,
        currentMode == TriggerMode::FallingEdge,
        [&settings]() {
            settings.triggerMode.setStoredValue(TriggerMode::FallingEdge);
        });
    targetMenu->addSubMenu("Trigger Mode", triggerMenu);

    // Trigger level presets
    juce::PopupMenu levelMenu;
    const float currentLevel = settings.triggerLevel.getStoredValue();
    const std::array<float, 5> levelPresets = {-0.5f, -0.1f, 0.0f, 0.1f, 0.5f};
    for (float preset : levelPresets) {
        levelMenu.addItem(juce::String(preset, 2),
            true,
            std::abs(preset - currentLevel) < 0.01f,
            [&settings, preset]() {
                settings.triggerLevel.setStoredValue(preset);
            });
    }
    targetMenu->addSubMenu("Trigger Level (" + juce::String(currentLevel, 2) + ")", levelMenu);

    // If title was provided, add the submenu to parent
    if (title.isNotEmpty()) {
        parentMenu.addSubMenu(title, scopeSubmenu);
    }
}

}  // namespace MoTool
