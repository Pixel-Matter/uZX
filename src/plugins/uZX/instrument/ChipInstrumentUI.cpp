#include "ChipInstrumentUI.h"
#include "ChipInstrumentPlugin.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"

namespace te = tracktion;

namespace MoTool::uZX {

static void setupRotarySlider(Slider& s, const String& tooltip, const String& name) {
    s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    s.setTooltip(tooltip);
    s.setName(name);
}

ChipInstrumentUI::ChipInstrumentUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
{
    setSize(200, 320);

    setupRotarySlider(adsrAttackSlider,   "Amp Attack Time",          "Attack");
    setupRotarySlider(adsrDecaySlider,    "Amp Decay Time",           "Decay");
    setupRotarySlider(adsrSustainSlider,  "Amp Sustain Level",        "Sustain");
    setupRotarySlider(adsrReleaseSlider,  "Amp Release Time",         "Release");
    setupRotarySlider(adsrVelocitySlider, "Amp Velocity Sensitivity", "Velocity");

    addAndMakeVisible(adsrAttackSlider);
    addAndMakeVisible(adsrDecaySlider);
    addAndMakeVisible(adsrSustainSlider);
    addAndMakeVisible(adsrReleaseSlider);
    // addAndMakeVisible(adsrVelocitySlider);

    adsrAttackLabel.setText("A", dontSendNotification);
    adsrDecayLabel.setText("D", dontSendNotification);
    adsrSustainLabel.setText("S", dontSendNotification);
    adsrReleaseLabel.setText("R", dontSendNotification);
    adsrVelocityLabel.setText("V", dontSendNotification);
    adsrAttackLabel.setJustificationType(Justification::centred);
    adsrDecayLabel.setJustificationType(Justification::centred);
    adsrSustainLabel.setJustificationType(Justification::centred);
    adsrReleaseLabel.setJustificationType(Justification::centred);
    adsrVelocityLabel.setJustificationType(Justification::centred);

    auto font = adsrAttackLabel.getFont().withPointHeight(10.0f);
    adsrAttackLabel.setFont(font);
    adsrDecayLabel.setFont(font);
    adsrSustainLabel.setFont(font);
    adsrReleaseLabel.setFont(font);
    adsrVelocityLabel.setFont(font);

    addAndMakeVisible(adsrAttackLabel);
    addAndMakeVisible(adsrDecayLabel);
    addAndMakeVisible(adsrSustainLabel);
    addAndMakeVisible(adsrReleaseLabel);
}

void ChipInstrumentUI::paint(Graphics& g) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void ChipInstrumentUI::resized() {
    auto r = getLocalBounds().reduced(8);
    static constexpr int knobSize = 40;
    static constexpr int spacing = 8;

    auto row = r.removeFromTop(knobSize);

    adsrAttackSlider.setBounds(row.removeFromLeft(knobSize));
    row.removeFromLeft(spacing);
    adsrDecaySlider.setBounds(row.removeFromLeft(knobSize));
    row.removeFromLeft(spacing);
    adsrSustainSlider.setBounds(row.removeFromLeft(knobSize));
    row.removeFromLeft(spacing);
    adsrReleaseSlider.setBounds(row.removeFromLeft(knobSize));
    // r.removeFromTop(spacing);
    // adsrVelocitySlider.setBounds(r.removeFromTop(knobSize));

    r.removeFromTop(0);
    row = r.removeFromTop(12);
    adsrAttackLabel.setBounds(adsrAttackSlider.getX(), row.getY(), knobSize, row.getHeight());
    adsrDecayLabel.setBounds(adsrDecaySlider.getX(), row.getY(), knobSize, row.getHeight());
    adsrSustainLabel.setBounds(adsrSustainSlider.getX(), row.getY(), knobSize, row.getHeight());
    adsrReleaseLabel.setBounds(adsrReleaseSlider.getX(), row.getY(), knobSize, row.getHeight());
    // adsrVelocityLabel.setBounds(adsrVelocitySlider.getX(), row.getY
}

REGISTER_PLUGIN_UI_ADAPTER(ChipInstrumentPlugin, ChipInstrumentUI)

}  // namespace MoTool::uZX
