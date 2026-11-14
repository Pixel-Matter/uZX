#include "AYPluginEditor.h"
#include "ChipClockPresets.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"
#include "../../../controllers/App.h"
#include "../../../models/PsgClip.h"


namespace MoTool::uZX {

namespace te = tracktion;

//==============================================================================
void drawStopwatch(Graphics& g, Rectangle<float> area, Colour colour) {
    const float size = jmin(area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre(size, size);
    const float stroke = jmax(1.0f, size / 10.0f);
    const float crown = size * 0.16f;  // the button on top
    const auto face = box.withTrimmedTop(crown).reduced(stroke * 0.5f)
                         .withSizeKeepingCentre(size - crown - stroke, size - crown - stroke);
    const auto centre = face.getCentre();
    const float radius = face.getWidth() * 0.5f;

    g.setColour(colour);
    g.drawEllipse(face, stroke);
    g.fillRect(Rectangle<float>(jmax(2.0f, size * 0.26f), crown).withCentre({centre.x, box.getY() + crown * 0.5f}));
    // the hand, a little past one o'clock
    g.drawLine(centre.x, centre.y, centre.x + radius * 0.42f, centre.y - radius * 0.5f, stroke);
}

//==============================================================================
// ChannelGroup implementation
//==============================================================================
void AYPluginUI::TimersToggle::paintButton(Graphics& g, bool highlighted, bool down) {
    auto& lf = getLookAndFeel();
    lf.drawButtonBackground(g, *this, findColour(getToggleState() ? buttonOnColourId : buttonColourId), highlighted, down);
    const auto colour = findColour(getToggleState() ? textColourOnId : textColourOffId).withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f);
    const float size = jmin(11.0f, static_cast<float>(jmin(getWidth(), getHeight())) - 4.0f);
    drawStopwatch(g, getLocalBounds().toFloat().withSizeKeepingCentre(size, size), colour);
}

AYPluginUI::ChannelGroup::ChannelGroup(AYChipPlugin& plugin,
                                        ParameterValue<bool>& channelParam,
                                        ParameterValue<bool>& toneParam,
                                        ParameterValue<bool>& noiseParam,
                                        ParameterValue<bool>& envelopeParam,
                                        ParameterValue<bool>& timersParam,
                                        ParameterValue<bool>& soloParam)
    : channelOn   { plugin, channelParam }
    , toneOn      { plugin, toneParam }
    , noiseOn     { plugin, noiseParam }
    , envelopeOn  { plugin, envelopeParam }
    , timersOn    { plugin, timersParam }
    , channelParam_(channelParam)
    , soloParam_(soloParam)
{
    timersOn.setComponentID("timers");
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
    parent.addChildComponent(timersOn);
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
    setupButtonColours(timersOn);
    updateSoloColour();

    // Set connected edges for buttons
    channelOn.setConnectedEdges(Button::ConnectedOnRight);
    toneOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnBottom);
    noiseOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnTop | Button::ConnectedOnBottom);
    envelopeOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnTop);
    timersOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnTop);
}

void AYPluginUI::ChannelGroup::setTimersShown(bool shown) {
    timersOn.setVisible(shown);
    envelopeOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnTop | (shown ? Button::ConnectedOnBottom : 0));
}

void AYPluginUI::ChannelGroup::layoutButtons(Rectangle<int> bounds) {
    const int buttonSize = bounds.getWidth() / 2;

    channelOn.setBounds(bounds.removeFromLeft(buttonSize));
    auto column = bounds.removeFromLeft(buttonSize);
    int rows = timersOn.isVisible() ? 4 : 3;
    toneOn.setBounds(column.removeFromTop(column.getHeight() / rows--));
    noiseOn.setBounds(column.removeFromTop(column.getHeight() / rows--));
    if (timersOn.isVisible())
        envelopeOn.setBounds(column.removeFromTop(column.getHeight() / rows--));
    (timersOn.isVisible() ? static_cast<Button&>(timersOn) : envelopeOn).setBounds(column);
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
    timersOn.setEnabled(enabled);
}

//==============================================================================
// AYScopes
//==============================================================================
AYScopes::AYScopes(AYChipPlugin& plugin, Look look) : plugin_(plugin), look_(look) {
    startTimerHz(20);

    static const std::array<juce::Colour, 3> colours {Colors::PSG::A, Colors::PSG::B, Colors::PSG::C};
    static const std::array<const char*, 3> labels {"A", "B", "C"};

    for (size_t i = 0; i < 3; ++i) {
        displays_[i] = std::make_unique<WaveformDisplay>(
            colours[i], labels[i], *plugin.getVizChannelBuffer(static_cast<int>(i)), plugin.scopeSettings);
        if (look_.classic)
            displays_[i]->setColours(Colors::VizClassic::trace, Colors::VizClassic::background);
        addAndMakeVisible(*displays_[i]);
    }
}

void AYScopes::paint(Graphics& g) {
    if (! look_.classic)
        return;
    // A rule in the middle of each gap, between the scopes
    g.setColour(Colors::VizClassic::rule);
    for (size_t i = 1; i < displays_.size(); ++i) {
        if (look_.sideBySide)
            g.fillRect(displays_[i]->getX() - spacing / 2, 0, 1, getHeight());
        else
            g.fillRect(0, displays_[i]->getY() - spacing / 2, getWidth(), 1);
    }
}

Rectangle<int> AYScopes::getTimerSignBounds(size_t chan) const {
    constexpr int size = 11, margin = 4;
    const auto scope = displays_[chan]->getBounds();
    return {scope.getRight() - size - margin, scope.getY() + margin, size, size};
}

uint8_t AYScopes::findTimerSigns() const {
    // Lit while a timer drives the channel; dimmed while the channel's timers switch is off
    const auto driven = plugin_.getTimerChannels();
    uint8_t signs = driven;
    for (size_t chan = 0; chan < 3; ++chan)
        if ((driven & (1u << chan)) != 0 && ! plugin_.channelMuter.getTimersParam(chan).getStoredValue())
            signs = static_cast<uint8_t>(signs | (0x10u << chan));
    return signs;
}

void AYScopes::timerCallback() {
    const auto signs = findTimerSigns();
    if (signs == shownTimerSigns_)
        return;
    shownTimerSigns_ = signs;
    for (size_t chan = 0; chan < displays_.size(); ++chan)
        repaint(getTimerSignBounds(chan));
}

void AYScopes::paintOverChildren(Graphics& g) {
    // Read here, not in the timer: a video frame is painted with no message loop running in between
    shownTimerSigns_ = findTimerSigns();
    static const std::array<juce::Colour, 3> colours {Colors::PSG::A, Colors::PSG::B, Colors::PSG::C};
    for (size_t chan = 0; chan < displays_.size(); ++chan) {
        if ((shownTimerSigns_ & (1u << chan)) == 0)
            continue;
        const bool dimmed = (shownTimerSigns_ & (0x10u << chan)) != 0;
        const auto colour = look_.classic ? Colors::VizClassic::trace : colours[chan];
        drawStopwatch(g, getTimerSignBounds(chan).toFloat(), colour.withAlpha(dimmed ? 0.35f : 1.0f));
    }
}

void AYScopes::resized() {
    auto r = getLocalBounds();
    const auto across = look_.sideBySide ? r.getWidth() : r.getHeight();
    const float size = ((float) across - spacing * 2.0f) / 3.0f;
    for (auto& display : displays_) {
        if (look_.sideBySide) {
            display->setBounds(r.removeFromLeft(roundToInt(size)));
            r.removeFromLeft(spacing);
        } else {
            display->setBounds(r.removeFromTop(roundToInt(size)));
            r.removeFromTop(spacing);
        }
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

    timerWarning_.setText(String::fromUTF8("\xE2\x9A\xA0"), dontSendNotification);  // ⚠
    timerWarning_.setJustificationType(Justification::centred);
    timerWarning_.setColour(Label::textColourId, Colors::Theme::warning);
    addChildComponent(timerWarning_);
    startTimerHz(4);
}

bool AYPluginUI::trackHasTimers() const {
    if (auto* track = dynamic_cast<te::ClipTrack*>(plugin_.getOwnerTrack()))
        for (auto* clip : track->getClips())
            if (auto* psg = dynamic_cast<PsgClip*>(clip); psg != nullptr && ! psg->getTimers().isEmpty())
                return true;
    return false;
}

void AYPluginUI::timerCallback() {
    const auto warnings = plugin_.getTimerWarnings();
    const bool timers = trackHasTimers();
    if (warnings == shownTimerWarnings_ && timers == timersShown_)
        return;
    shownTimerWarnings_ = warnings;
    timerWarning_.setTooltip(AYChipPlugin::describeTimerWarnings(warnings));
    timerWarning_.setVisible(warnings != 0);
    // The timers switches are there while the track has a clip with timers.
    // ponytail: hidden for now, a fourth row makes T, N and E too small; their parameters still automate.
    // Show them again once the channel group has room for them.
    timersShown_ = timers;
    // for (auto* group : channelGroups)
    //     group->setTimersShown(timers);
    resized();
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
    const auto buttonWidth = jmin(staticRow.getWidth() / 4 - 8, maxButtonWidth);
    chipTypeButton.setBounds(staticRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, itemHeight));
    staticRow.removeFromLeft(8);
    if (timerWarning_.isVisible())
        timerWarning_.setBounds(staticRow.removeFromRight(itemHeight));
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
    addScopeSettingsMenu(menu, plugin_.scopeSettings, "Scope Settings");
}

REGISTER_PLUGIN_UI_ADAPTER(AYChipPlugin, AYPluginUI)

}
