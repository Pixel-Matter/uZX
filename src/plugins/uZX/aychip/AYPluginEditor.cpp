#include "AYPluginEditor.h"
#include "ChipClockPresets.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"


namespace MoTool::uZX {

namespace te = tracktion;

//==============================================================================
// ChannelGroup implementation
//==============================================================================
AYPluginUI::ChannelGroup::ChannelGroup(AYChipPlugin& plugin,
                                        ParameterValue<bool>& channelParam,
                                        ParameterValue<bool>& toneParam,
                                        ParameterValue<bool>& noiseParam,
                                        ParameterValue<bool>& envelopeParam)
    : channelOn   { plugin, channelParam }
    , toneOn      { plugin, toneParam }
    , noiseOn     { plugin, noiseParam }
    , envelopeOn  { plugin, envelopeParam }
    , channelParam_(channelParam)
{
    channelParam_.addListener(this);
    updateTNEState();
}

AYPluginUI::ChannelGroup::~ChannelGroup() {
    channelParam_.removeListener(this);
}

void AYPluginUI::ChannelGroup::addToComponent(Component& parent) {
    parent.addAndMakeVisible(channelOn);
    parent.addAndMakeVisible(toneOn);
    parent.addAndMakeVisible(noiseOn);
    parent.addAndMakeVisible(envelopeOn);
}

void AYPluginUI::ChannelGroup::setupButtonColours(ToggleButton& button) {
    // button .setColour(TextButton::buttonOnColourId, Colors::Theme::soloed);

    // if logic is inverted, use Theme::surface as on color
    button.setColour(TextButton::buttonOnColourId, Colors::Theme::surface);
    button.setColour(TextButton::buttonColourId,   Colors::Theme::muted);
    button.setColour(TextButton::textColourOffId,  Colors::Theme::background);
    button.setColour(TextButton::textColourOnId,   Colors::Theme::textPrimary);
}

void AYPluginUI::ChannelGroup::setupWidgets() {
    setupButtonColours(channelOn);
    setupButtonColours(toneOn);
    setupButtonColours(noiseOn);
    setupButtonColours(envelopeOn);

    // Set connected edges for buttons
    channelOn.setConnectedEdges(Button::ConnectedOnRight);
    toneOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnBottom);
    noiseOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnTop | Button::ConnectedOnBottom);
    envelopeOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnTop);
}

void AYPluginUI::ChannelGroup::layoutButtons(Rectangle<int> bounds) {
    const int buttonSize = bounds.getWidth() / 2;

    channelOn.setBounds(bounds.removeFromLeft(buttonSize));
    auto column = bounds.removeFromLeft(buttonSize);
    toneOn.setBounds(column.removeFromTop(column.getHeight() / 3));
    noiseOn.setBounds(column.removeFromTop(column.getHeight() / 2));
    envelopeOn.setBounds(column);
}

void AYPluginUI::ChannelGroup::valueChanged(Value&) {
    updateTNEState();
}

void AYPluginUI::ChannelGroup::updateTNEState() {
    bool enabled = channelParam_.getStoredValue();
    toneOn.setEnabled(enabled);
    noiseOn.setEnabled(enabled);
    envelopeOn.setEnabled(enabled);
}

//==============================================================================
// Editor for AYChipPlugin
//==============================================================================
AYPluginUI::AYPluginUI(te::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , plugin_(*dynamic_cast<AYChipPlugin*>(pluginPtr.get()))
    , chipClockBinding(chipClockCombo, plugin_.staticParams.chipClock, makeChipClockPresets(), false, true)
{
    constrainer_.setMinimumWidth(224);
    setSize(224, 180);

    addAndMakeVisible(chipTypeButton);
    addAndMakeVisible(chipClockCombo);
    chipClockBinding.setDecimalPlaces(2);

    addAndMakeVisible(volumeKnob);
    addAndMakeVisible(layoutButton);
    addAndMakeVisible(stereoKnob);

    setupToggleButtons();

    // Initialize scope displays for channels A, B, C
    static const std::array<juce::Colour, 3> channelColours{
        Colors::PSG::A,  // Blue
        Colors::PSG::B,  // Emerald
        Colors::PSG::C   // Amber
    };
    static const std::array<const char*, 3> channelLabels{"A", "B", "C"};

    for (size_t i = 0; i < 3; ++i) {
        scopeDisplays_[i] = std::make_unique<WaveformDisplay>(
            channelColours[i],
            juce::String(channelLabels[i])
        );
        scopeDisplays_[i]->setBuffer(plugin_.getVizChannelBuffer(static_cast<int>(i)));
        scopeDisplays_[i]->setScopeSettings(&plugin_.scopeSettings);
        addAndMakeVisible(*scopeDisplays_[i]);
    }

    // Listen to scope settings changes
    plugin_.scopeSettings.windowSamples.addListener(this);
    plugin_.scopeSettings.gain.addListener(this);
    plugin_.scopeSettings.triggerMode.addListener(this);
    plugin_.scopeSettings.triggerLevel.addListener(this);

    // Apply initial scope settings to all displays
    updateScopeDisplayParams();
}

AYPluginUI::~AYPluginUI() {
    plugin_.scopeSettings.windowSamples.removeListener(this);
    plugin_.scopeSettings.gain.removeListener(this);
    plugin_.scopeSettings.triggerMode.removeListener(this);
    plugin_.scopeSettings.triggerLevel.removeListener(this);
}

void AYPluginUI::valueChanged(juce::Value&) {
    updateScopeDisplayParams();
}

void AYPluginUI::updateScopeDisplayParams() {
    const int window = plugin_.scopeSettings.windowSamples.getStoredValue();
    const float gain = plugin_.scopeSettings.gain.getStoredValue();
    const auto mode = plugin_.scopeSettings.triggerMode.getStoredValue();
    const float level = plugin_.scopeSettings.triggerLevel.getStoredValue();

    auto strategy = TriggerStrategy::fromMode(mode);

    // Apply settings to all 3 scope displays
    for (auto& display : scopeDisplays_) {
        if (display) {
            display->setWindowSize(window);
            display->setGain(gain);
            display->setTriggerStrategy(strategy);
            display->setTriggerLevel(level);
        }
    }
}

void AYPluginUI::setupToggleButtons() {
    for (auto* group : channelGroups) {
        group->addToComponent(*this);
        group->setupWidgets();
    }
}

void AYPluginUI::paint(Graphics& g) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void AYPluginUI::resized() {
    auto r = getLocalBounds().reduced(8, 8);

    // static
    auto staticRow = r.removeFromTop(itemHeight);
    auto buttonWidth = staticRow.getWidth() / 4 - 8;
    chipTypeButton.setBounds(staticRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, 20));
    staticRow.removeFromLeft(8);
    chipClockCombo.setBounds(staticRow.withSizeKeepingCentre(staticRow.getWidth(), 20));

    // automatable
    r.removeFromTop(itemSpacing * 2);
    auto knobsRow = r.removeFromTop(itemHeight * 2);

    layoutButton.setBounds(knobsRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, 20));

    knobsRow.removeFromLeft(8);
    stereoKnob.setBounds(knobsRow.removeFromLeft(knobsRow.getWidth() / 2));
    volumeKnob.setBounds(knobsRow);

    // Channel and effect toggles + scope displays
    r.removeFromTop(itemSpacing * 2);
    auto col = r.removeFromLeft(buttonWidth);
    const float h = ((float) col.getHeight() - itemSpacing * 2.0f) / 3.0f;

    // Add spacing between channel groups and scopes
    r.removeFromLeft(itemSpacing);

    // Layout scope displays to the right of channel groups
    auto scopeArea = r;

    for (int i = 0; i < 3; ++i) {
        channelGroups[i]->layoutButtons(col.removeFromTop(roundToInt(h)));

        // Layout corresponding scope display
        auto scopeBounds = scopeArea.removeFromTop(roundToInt(h));
        if (scopeDisplays_[static_cast<size_t>(i)]) {
            scopeDisplays_[static_cast<size_t>(i)]->setBounds(scopeBounds);
        }

        col.removeFromTop(itemSpacing);
        scopeArea.removeFromTop(itemSpacing);
    }
}

ComponentBoundsConstrainer* AYPluginUI::getBoundsConstrainer() {
    return &constrainer_;
}

bool AYPluginUI::hasDeviceMenu() const {
    return true;
}

void AYPluginUI::populateDeviceMenu(juce::PopupMenu& menu) {
    addMidiRangeMenu(menu, plugin_.staticParams.baseMidiChannel, plugin_.staticParams.baseMidiChannel.definition.description, 4);
    // addDiscreteIntegerParameterMenu(menu, plugin_.staticParams.numOutputChannels, plugin_.staticParams.numOutputChannels.definition.description);

    menu.addSeparator();

    // Scope settings submenu
    juce::PopupMenu scopeMenu;

    // Window size presets
    juce::PopupMenu windowMenu;
    const std::array<int, 5> windowPresets = {256, 512, 1024, 2048, 4096};
    for (int preset : windowPresets) {
        windowMenu.addItem(juce::String(preset) + " samples",
            [this, preset]() {
                plugin_.scopeSettings.windowSamples.setStoredValue(preset);
            });
    }
    scopeMenu.addSubMenu("Window Size", windowMenu);

    // Gain presets
    juce::PopupMenu gainMenu;
    const std::array<float, 5> gainPresets = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    for (float preset : gainPresets) {
        gainMenu.addItem(juce::String(preset, 1) + "x",
            [this, preset]() {
                plugin_.scopeSettings.gain.setStoredValue(preset);
            });
    }
    scopeMenu.addSubMenu("Gain", gainMenu);

    // Trigger mode
    juce::PopupMenu triggerMenu;
    triggerMenu.addItem("Free Running",
        [this]() {
            plugin_.scopeSettings.triggerMode.setStoredValue(TriggerMode::FreeRunning);
        });
    triggerMenu.addItem("Rising Edge",
        [this]() {
            plugin_.scopeSettings.triggerMode.setStoredValue(TriggerMode::RisingEdge);
        });
    triggerMenu.addItem("Falling Edge",
        [this]() {
            plugin_.scopeSettings.triggerMode.setStoredValue(TriggerMode::FallingEdge);
        });
    scopeMenu.addSubMenu("Trigger Mode", triggerMenu);

    // Trigger level presets
    juce::PopupMenu levelMenu;
    const std::array<float, 5> levelPresets = {-0.5f, -0.1f, 0.0f, 0.1f, 0.5f};
    for (float preset : levelPresets) {
        levelMenu.addItem(juce::String(preset, 2),
            [this, preset]() {
                plugin_.scopeSettings.triggerLevel.setStoredValue(preset);
            });
    }
    scopeMenu.addSubMenu("Trigger Level", levelMenu);

    menu.addSubMenu("Scope Settings", scopeMenu);
}

REGISTER_PLUGIN_UI_ADAPTER(AYChipPlugin, AYPluginUI)

}
