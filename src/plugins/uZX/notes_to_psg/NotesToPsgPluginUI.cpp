#include "NotesToPsgPluginUI.h"
#include "../aychip/ChipClockPresets.h"
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
{
    plugin.staticParams.tuningTable.addListener(this);

    refTuningLabel.setText("Reference tuning:", dontSendNotification);
    refTuningLabel.setJustificationType(Justification::centredLeft);
    refTuningLabel.setFont(FontOptions().withPointHeight(11.0f));

    update();

    ui.addAndMakeVisible(chipClock);
    ui.addAndMakeVisible(a4Frequency);
    ui.addAndMakeVisible(tuningType);
    ui.addAndMakeVisible(refTuningLabel);
    ui.addAndMakeVisible(refTuning);
    ui.addAndMakeVisible(tonicAndScale);
}

NotesToPsgPluginUI::InfoGroup::~InfoGroup() {
    plugin.staticParams.tuningTable.removeListener(this);
}

void NotesToPsgPluginUI::InfoGroup::valueChanged(Value& value) {
    ignoreUnused(value);
    update();
}

void NotesToPsgPluginUI::InfoGroup::resize(Rectangle<int>& r) {
    refTuningLabel.setBounds(r.removeFromTop(itemHeight));
    // r.removeFromTop(spacing);
    refTuning.setBounds(r.removeFromTop(itemHeight));
    r.removeFromTop(spacing);

    tuningType.setBounds(r.removeFromTop(itemHeight));
    r.removeFromTop(spacing);
    chipClock.setBounds(r.removeFromTop(itemHeight));
    r.removeFromTop(spacing);
    a4Frequency.setBounds(r.removeFromTop(itemHeight));
    r.removeFromTop(spacing);
    tonicAndScale.setBounds(r.removeFromTop(itemHeight));
}

void NotesToPsgPluginUI::InfoGroup::update() {
    const auto& tuning = plugin.getTuningSystem();
    auto referenceTuning = tuning.getReferenceTuning();

    refTuning.setText(referenceTuning->getType().getLongLabel().data(), dontSendNotification);
    tuningType.setText("Tuning type: " + String(tuning.getType().getLongLabel().data()), dontSendNotification);
    chipClock.setText("Chip clock: " + String(tuning.getClockFrequency() / MHz, 4) + " MHz", dontSendNotification);
    a4Frequency.setText("A4 frequency: " + String(referenceTuning->getA4Frequency(), 2) + " Hz", dontSendNotification);

    bool isRationalTuning = dynamic_cast<RationalTuning*>(referenceTuning) != nullptr;
    tonicAndScale.setVisible(isRationalTuning);

    if (isRationalTuning) {
        auto* rationalTuning = dynamic_cast<RationalTuning*>(referenceTuning);
        tonicAndScale.setText(
            "Key: " +
            String::fromUTF8(rationalTuning->getTonic().getName()) + " " +
            String::fromUTF8(rationalTuning->getScaleType().getLabel().data()),
            dontSendNotification
        );
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
