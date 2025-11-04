#include "NotesToPsgPluginUI.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"
#include "../../../models/tuning/TemperamentSystem.h"

namespace MoTool::uZX {

//==============================================================================
// NotesToPsgPluginUI
//==============================================================================

NotesToPsgPluginUI::TuningGroup::TuningGroup(NotesToPsgPluginUI& ui)
    : binding(combo, ui.notesToPsgPlugin()->staticParams.tuningTable)
{
    label.setText("Tuning preset:", dontSendNotification);
    label.setJustificationType(Justification::centredLeft);
    label.setFont(FontOptions().withPointHeight(11.0f));
    label.setColour(Label::textColourId, Colors::Theme::textSecondary);

    ui.addAndMakeVisible(label);
    ui.addAndMakeVisible(combo);
}

void NotesToPsgPluginUI::TuningGroup::resize(Rectangle<int>& r) {
    label.setBounds(r.removeFromTop(itemHeight));
    // r.removeFromTop(spacing);
    combo.setBounds(r.removeFromTop(itemHeight));
}

//==============================================================================
NotesToPsgPluginUI::InfoGroup::InfoGroup(NotesToPsgPluginUI& ui)
    : plugin(*ui.notesToPsgPlugin())
    , parentUI(ui)
{
    plugin.staticParams.tuningTable.addListener(this);

    tuningType.setFont(FontOptions().withPointHeight(11.0f));
    tuningType.setColour(Label::textColourId, Colors::Theme::textSecondary);

    chipClockLabel.setText("Chip clock:", dontSendNotification);
    chipClockLabel.setJustificationType(Justification::centredLeft);
    chipClockLabel.setFont(FontOptions().withPointHeight(11.0f));
    chipClockLabel.setColour(Label::textColourId, Colors::Theme::textSecondary);

    a4FrequencyLabel.setText("A4 frequency:", dontSendNotification);
    a4FrequencyLabel.setJustificationType(Justification::centredLeft);
    a4FrequencyLabel.setFont(FontOptions().withPointHeight(11.0f));
    a4FrequencyLabel.setColour(Label::textColourId, Colors::Theme::textSecondary);

    ui.addAndMakeVisible(tuningType);
    ui.addAndMakeVisible(refTuning);
    ui.addAndMakeVisible(tonicAndScale);
    ui.addAndMakeVisible(chipClockLabel);
    ui.addAndMakeVisible(chipClock);
    ui.addAndMakeVisible(a4FrequencyLabel);
    ui.addAndMakeVisible(a4Frequency);

    update();
}

NotesToPsgPluginUI::InfoGroup::~InfoGroup() {
    plugin.staticParams.tuningTable.removeListener(this);
}

void NotesToPsgPluginUI::InfoGroup::valueChanged(Value& value) {
    ignoreUnused(value);
    update();
}

void NotesToPsgPluginUI::InfoGroup::resize(Rectangle<int>& r) {
    tuningType.setBounds(r.removeFromTop(itemHeight));
    refTuning.setBounds(r.removeFromTop(itemHeight));

    if (tonicAndScale.isVisible()) {
        tonicAndScale.setBounds(r.removeFromTop(itemHeight));
    }

    r.removeFromTop(spacing);
    chipClockLabel.setBounds(r.removeFromTop(itemHeight));
    chipClock.setBounds(r.removeFromTop(itemHeight));
    r.removeFromTop(spacing);
    a4FrequencyLabel.setBounds(r.removeFromTop(itemHeight));
    a4Frequency.setBounds(r.removeFromTop(itemHeight));
}

void NotesToPsgPluginUI::InfoGroup::update() {
    const auto& tuning = plugin.getTuningSystem();
    auto referenceTuning = tuning.getReferenceTuning();

    tuningType.setText(String(tuning.getType().getLongLabel().data()) + " to:", dontSendNotification);
    refTuning.setText(referenceTuning->getType().getLongLabel().data(), dontSendNotification);
    chipClock.setText(String(tuning.getClockFrequency() / MHz, 4) + " MHz", dontSendNotification);
    a4Frequency.setText(String(referenceTuning->getA4Frequency(), 2) + " Hz", dontSendNotification);

    bool isRationalTuning = dynamic_cast<RationalTuning*>(referenceTuning) != nullptr;
    bool wasVisible = tonicAndScale.isVisible();
    tonicAndScale.setVisible(isRationalTuning);

    if (isRationalTuning) {
        auto* rationalTuning = dynamic_cast<RationalTuning*>(referenceTuning);
        tonicAndScale.setText(
            // "Key: " +
            String::fromUTF8(rationalTuning->getTonic().getName()) + " " +
            String::fromUTF8(rationalTuning->getScaleType().getLabel().data()),
            dontSendNotification
        );
    }

    // Trigger resize if visibility changed
    if (wasVisible != tonicAndScale.isVisible()) {
        parentUI.resized();
    }
}

//==============================================================================
NotesToPsgPluginUI::NotesToPsgPluginUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , staticParams(notesToPsgPlugin()->staticParams)
    , tuning(*this)
    , tuningInfo(*this)
{
    jassert(pluginPtr != nullptr);
    setSize(192, 148);
    notesToPsgPlugin()->addChangeListener(this);
}

NotesToPsgPluginUI::~NotesToPsgPluginUI() {
    notesToPsgPlugin()->removeChangeListener(this);
}

NotesToPsgPlugin* NotesToPsgPluginUI::notesToPsgPlugin() const {
    return dynamic_cast<NotesToPsgPlugin*>(plugin.get());
}

bool NotesToPsgPluginUI::hasDeviceMenu() const {
    return notesToPsgPlugin() != nullptr;
}

void NotesToPsgPluginUI::populateDeviceMenu(juce::PopupMenu& menu) {
    if (notesToPsgPlugin() == nullptr) {
        return;
    }

    addMidiRangeMenu(menu, staticParams.baseMidiChannel, staticParams.baseMidiChannel.definition.description, 4);
}

void NotesToPsgPluginUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void NotesToPsgPluginUI::resized() {
    auto r = getLocalBounds().reduced(8, 4);

    tuning.resize(r);
    r.removeFromTop(spacing);
    tuningInfo.resize(r);
}

void NotesToPsgPluginUI::changeListenerCallback(ChangeBroadcaster* source) {
    ignoreUnused(source);
    tuningInfo.update();
}

REGISTER_PLUGIN_UI_ADAPTER(NotesToPsgPlugin, NotesToPsgPluginUI)

}  // namespace MoTool::uZX
