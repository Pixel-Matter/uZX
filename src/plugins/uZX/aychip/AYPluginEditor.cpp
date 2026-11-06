#include "AYPluginEditor.h"
#include "ChipClockPresets.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"
#include "../../../controllers/App.h"


namespace MoTool::uZX {

namespace te = tracktion;

//==============================================================================
// ChannelGroup implementation
//==============================================================================
AYPluginUI::ChannelGroup::ChannelGroup(AYChipPlugin& plugin,
                                        ParameterValue<bool>& channelParam,
                                        ParameterValue<bool>& toneParam,
                                        ParameterValue<bool>& noiseParam,
                                        ParameterValue<bool>& envelopeParam,
                                        ParameterValue<bool>& soloParam)
    : channelOn   { plugin, channelParam }
    , toneOn      { plugin, toneParam }
    , noiseOn     { plugin, noiseParam }
    , envelopeOn  { plugin, envelopeParam }
    , channelParam_(channelParam)
    , soloParam_(soloParam)
{
    channelParam_.addListener(this);
    soloParam_.addListener(this);
    updateTNEState();
}

AYPluginUI::ChannelGroup::~ChannelGroup() {
    channelParam_.removeListener(this);
    soloParam_.removeListener(this);
}

void AYPluginUI::ChannelGroup::addToComponent(Component& parent) {
    parent.addAndMakeVisible(channelOn);
    parent.addAndMakeVisible(toneOn);
    parent.addAndMakeVisible(noiseOn);
    parent.addAndMakeVisible(envelopeOn);
}

void AYPluginUI::ChannelGroup::setupButtonColours(ToggleButton& button) {
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
    updateSoloColour();

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
    updateSoloColour();
}

void AYPluginUI::ChannelGroup::updateSoloColour() {
    // a soloed channel plays, muted or not
    const bool soloed = soloParam_.getStoredValue();
    channelOn.setColour(TextButton::buttonOnColourId, soloed ? Colors::Theme::soloed : Colors::Theme::surface);
    channelOn.setColour(TextButton::buttonColourId, soloed ? Colors::Theme::soloed : Colors::Theme::muted);
    channelOn.setColour(TextButton::textColourOffId, soloed ? Colors::Theme::textPrimary : Colors::Theme::background);
}

void AYPluginUI::ChannelGroup::updateTNEState() {
    bool enabled = channelParam_.getStoredValue() || soloParam_.getStoredValue();
    toneOn.setEnabled(enabled);
    noiseOn.setEnabled(enabled);
    envelopeOn.setEnabled(enabled);
}

//==============================================================================
// AYScopes
//==============================================================================
AYScopes::AYScopes(AYChipPlugin& plugin) : settings_(plugin.scopeSettings) {
    static const std::array<juce::Colour, 3> colours {Colors::PSG::A, Colors::PSG::B, Colors::PSG::C};
    static const std::array<const char*, 3> labels {"A", "B", "C"};

    for (size_t i = 0; i < 3; ++i) {
        displays_[i] = std::make_unique<WaveformDisplay>(colours[i], labels[i],
                                                         *plugin.getVizChannelBuffer(static_cast<int>(i)), settings_);
        addAndMakeVisible(*displays_[i]);
    }

    settings_.windowSamples.addListener(this);
    settings_.gain.addListener(this);
    settings_.triggerMode.addListener(this);
    settings_.triggerLevel.addListener(this);
    applySettings();
}

AYScopes::~AYScopes() {
    settings_.windowSamples.removeListener(this);
    settings_.gain.removeListener(this);
    settings_.triggerMode.removeListener(this);
    settings_.triggerLevel.removeListener(this);
}

void AYScopes::valueChanged(juce::Value&) {
    applySettings();
}

void AYScopes::applySettings() {
    const auto strategy = TriggerStrategy::fromMode(settings_.triggerMode.getStoredValue());
    for (auto& display : displays_) {
        display->setWindowSize(settings_.windowSamples.getStoredValue());
        display->setGain(settings_.gain.getStoredValue());
        display->setTriggerStrategy(strategy);
        display->setTriggerLevel(settings_.triggerLevel.getStoredValue());
    }
}

void AYScopes::resized() {
    auto r = getLocalBounds();
    const float h = ((float) r.getHeight() - spacing * 2.0f) / 3.0f;
    for (auto& display : displays_) {
        display->setBounds(r.removeFromTop(roundToInt(h)));
        r.removeFromTop(spacing);
    }
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

    addAndMakeVisible(scopes_);
}

AYPluginUI::~AYPluginUI() = default;

void AYPluginUI::setupToggleButtons() {
    for (size_t chan = 0; chan < channelGroups.size(); ++chan) {
        auto& group = *channelGroups[chan];
        group.addToComponent(*this);
        group.setupWidgets();

        auto& button = group.channelOn;
       #if JUCE_MAC
        button.setTooltip(button.getTooltip() + ". Option-click: solo");
       #else
        button.setTooltip(button.getTooltip() + ". Alt-click: solo");
       #endif
        // a channel is on, muted or soloed: a click turns a muted or soloed one back on
        button.onClick = [this, chan, toggle = std::move(button.onClick)] {
            if (ModifierKeys::currentModifiers.isAltDown() || plugin_.channelMuter.getSoloParam(chan).getStoredValue())
                plugin_.toggleSolo(chan);
            else
                toggle();
        };
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
    chipTypeButton.setBounds(staticRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, itemHeight));
    staticRow.removeFromLeft(8);
    chipClockCombo.setBounds(staticRow.withSizeKeepingCentre(staticRow.getWidth(), 20));

    // automatable
    r.removeFromTop(itemSpacing * 2);
    auto knobsRow = r.removeFromTop(itemHeight * 2);

    layoutButton.setBounds(knobsRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, itemHeight));

    knobsRow.removeFromLeft(8);
    stereoKnob.setBounds(knobsRow.removeFromLeft(knobsRow.getWidth() / 2));
    volumeKnob.setBounds(knobsRow);

    // Channel and effect toggles + scope displays
    r.removeFromTop(itemSpacing * 2);
    auto col = r.removeFromLeft(buttonWidth);
    const float h = ((float) col.getHeight() - itemSpacing * 2.0f) / 3.0f;

    for (auto* group : channelGroups) {
        group->layoutButtons(col.removeFromTop(roundToInt(h)));
        col.removeFromTop(itemSpacing);
    }

    // the scopes to the right, level with the channel groups
    r.removeFromLeft(itemSpacing * 2);
    scopes_.setBounds(r);
}

int AYPluginUI::getMinimumHeight() const {
    // as resized() lays it out: margins, the chip row, the knobs, then three channel groups of T, N and E
    constexpr int margins = 2 * 8, gaps = 2 * (2 * itemSpacing);
    constexpr int channels = 3 * (3 * minToggleHeight) + 2 * itemSpacing;
    return margins + itemHeight + gaps + 2 * itemHeight + channels;
}

ComponentBoundsConstrainer* AYPluginUI::getBoundsConstrainer() {
    return &constrainer_;
}

bool AYPluginUI::hasDeviceMenu() const {
    return true;
}

void AYPluginUI::populateDeviceMenu(juce::PopupMenu& menu) {
    if (MoToolApp::hasMidiControl())
        addMidiRangeMenu(menu, plugin_.staticParams.baseMidiChannel, plugin_.staticParams.baseMidiChannel.definition.description, 4);
    // addDiscreteIntegerParameterMenu(menu, plugin_.staticParams.numOutputChannels, plugin_.staticParams.numOutputChannels.definition.description);

   #if JUCE_DEBUG  // its output goes through DBG, which Release builds drop
    const bool monitorEnabled = plugin_.staticParams.monitorMode.getStoredValue();
    menu.addItem(plugin_.staticParams.monitorMode.definition.description,
                 true,
                 monitorEnabled,
                 [this]() {
                     plugin_.staticParams.monitorMode.setStoredValue(
                         !plugin_.staticParams.monitorMode.getStoredValue());
                 });
   #endif

    menu.addSeparator();

    // Add scope settings as submenu
    MoTool::addScopeSettingsMenu(menu, plugin_.scopeSettings, "Scope Settings");
}

REGISTER_PLUGIN_UI_ADAPTER(AYChipPlugin, AYPluginUI)

}
