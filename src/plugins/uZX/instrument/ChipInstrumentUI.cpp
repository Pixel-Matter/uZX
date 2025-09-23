#include "ChipInstrumentUI.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"

namespace te = tracktion;

namespace MoTool::uZX {

LabeledRotarySlider::LabeledRotarySlider(te::AutomatableParameter::Ptr param, const String& labelText, const String& tooltip, const String& valueSuffix)
    : attachment(slider, *param), parameter(param), mouseListener(*this)
{
    slider.setSliderStyle(Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setTooltip(tooltip);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    slider.setNumDecimalPlacesToDisplay(2);
    slider.setTextValueSuffix(valueSuffix);

    // Disable default popup menu and add custom mouse listener for right-click
    slider.setPopupMenuEnabled(false);
    slider.addMouseListener(&mouseListener, false);

    addAndMakeVisible(slider);

    label.setText(labelText, dontSendNotification);
    label.setJustificationType(Justification::centred);
    label.setFont(label.getFont().withPointHeight((float) labelHeight));
    addAndMakeVisible(label);
}

LabeledRotarySlider::LabeledRotarySlider(te::AutomatableParameter::Ptr param, const ValueWithSource<float>& value)
    : LabeledRotarySlider(param, value.definition.shortLabel, value.definition.description, value.definition.units)
{}

LabeledRotarySlider::LabeledRotarySlider(const ValueWithSource<float>& value)
    : LabeledRotarySlider(*value.source->parameter, value.definition.shortLabel, value.definition.description, value.definition.units)
{
    jassert(value.source != nullptr);
}


void LabeledRotarySlider::resized() {
    auto bounds = getLocalBounds();
    auto sliderHeight = bounds.getHeight() - labelHeight;

    slider.setBounds(bounds.removeFromTop(sliderHeight));
    bounds.translate(0, -labelOverlap);
    label.setBounds(bounds);
}

void LabeledRotarySlider::paint(Graphics& g) {
    // Draw a small indicator in top-left corner if parameter is mapped to MIDI CC
    if (isParameterMapped()) {
        g.setColour(Colours::orange);
        g.fillRect(0, 0, 4, 4);
    }
}


void LabeledRotarySlider::showParameterMenu() {
    if (!parameter) return;

    PopupMenu menu;

    // Check if parameter is currently mapped
    bool isMapped = isParameterMapped();

    menu.addSectionHeader("MIDI mapping");
    menu.addSeparator();

    if (isMapped) {
        // Show current mapping as inactive item
        menu.addItem(0, getMappingDescription(), false); // inactive
        menu.addItem(1, "Clear");
        menu.addSeparator();
    }

    menu.addItem(2, "Learn...");
    menu.addSeparator();

    // Add "Map MIDI >" submenu
    PopupMenu midiSubmenu = createMidiMappingSubmenu();
    menu.addSubMenu("Map to >", midiSubmenu);

    auto result = menu.show();

    if (result == 1) {
        clearMidiMapping();
        repaint(); // Update visual feedback
    } else if (result == 2) {
        learnMidiCC();
    } else if (result >= 1000) {
        // Handle direct MIDI CC mapping (result IDs start from 1000)
        int controllerID = result - 1000;
        mapMidiCC(controllerID);
        repaint(); // Update visual feedback
    }
}

void LabeledRotarySlider::learnMidiCC() {
    if (!parameter) return;

    // Get the Edit from the parameter
    auto& edit = parameter->getEdit();
    auto& mappings = edit.getParameterControlMappings();

    // Put the mappings into learn mode
    // Find an available row or add a new one
    int rowIndex = mappings.getNumControllerIDs();
    mappings.listenToRow(rowIndex);

    // Show a modal dialog or temporary message
    AlertWindow::showMessageBoxAsync(
        AlertWindow::InfoIcon,
        "MIDI Learn",
        "Move a MIDI controller now to assign it to \"" + parameter->getFullName() + "\".\\n\\nPress any key or click OK to cancel.",
        "OK"
    );
    mappings.setLearntParam(false);
    mappings.saveToEdit();
}

void LabeledRotarySlider::clearMidiMapping() {
    if (!parameter) return;

    auto& edit = parameter->getEdit();
    auto& mappings = edit.getParameterControlMappings();
    mappings.removeParameterMapping(*parameter);
}

bool LabeledRotarySlider::isParameterMapped() const {
    if (!parameter) return false;

    auto& edit = parameter->getEdit();
    auto& mappings = edit.getParameterControlMappings();
    return mappings.isParameterMapped(*parameter);
}

String LabeledRotarySlider::getMappingDescription() const {
    if (!parameter) return {};

    auto& edit = parameter->getEdit();
    auto& mappings = edit.getParameterControlMappings();

    int channel, controllerID;
    if (mappings.getParameterMapping(*parameter, channel, controllerID)) {
        return formatControllerDescription(controllerID, channel);
    }

    return "Unknown";
}

void LabeledRotarySlider::mapMidiCC(int controllerID, int channel) {
    if (!parameter) return;

    auto& edit = parameter->getEdit();
    auto& mappings = edit.getParameterControlMappings();

    // Remove existing mapping first
    mappings.removeParameterMapping(*parameter);

    // Manually craft the ValueTree state for MIDI mapping
    // Based on ParameterControlMappings::saveToEdit() structure
    auto um = &edit.getUndoManager();
    auto state = edit.state.getOrCreateChildWithName(tracktion::IDs::CONTROLLERMAPPINGS, um);

    // Create mapping entry using Tracktion's format
    auto mappingNode = tracktion::createValueTree(tracktion::IDs::MAP,
                                                  tracktion::IDs::id, controllerID,
                                                  tracktion::IDs::channel, channel,
                                                  tracktion::IDs::param, parameter->getFullName(),
                                                  tracktion::IDs::pluginID, parameter->getOwnerID());

    // Add the mapping to the state
    state.addChild(mappingNode, -1, um);

    // Force reload from the state to activate the mapping
    mappings.loadFromEdit();

    DBG("Manually mapped " << parameter->getParameterName()
        << " to controller " << controllerID << " on channel " << channel
        << " - " << getMappingDescription());
}

// Helper function to format CC names using JUCE's built-in names
String LabeledRotarySlider::formatCCName(int ccNumber) {
    auto name = String(MidiMessage::getControllerName(ccNumber));
    String result = "CC " + String(ccNumber);

    if (name.isNotEmpty()) {
        result += " (" + name + ")";
    }

    return result;
}

// Standalone function to decode and format controller descriptions
String LabeledRotarySlider::formatControllerDescription(int controllerID, int channel) {
    // Decode controller ID (from ParameterControlMappings.cpp format)
    if (controllerID >= 0x40000) {
        return "Channel Pressure [" + String(channel) + "]";
    } else if (controllerID >= 0x30000) {
        return "RPN #" + String(controllerID & 0x7fff) + " [" + String(channel) + "]";
    } else if (controllerID >= 0x20000) {
        return "NRPN #" + String(controllerID & 0x7fff) + " [" + String(channel) + "]";
    } else if (controllerID >= 0x10000) {
        auto ccNum = controllerID & 0x7f;
        auto nameStr = String(MidiMessage::getControllerName(ccNum));
        String desc = "CC #" + String(ccNum);
        if (nameStr.isNotEmpty()) {
            desc += " (" + nameStr + ")";
        }
        desc += " [" + String(channel) + "]";
        return desc;
    }

    return "Unknown Controller [" + String(channel) + "]";
}

// Create submenu for a range of CC numbers (16 CCs per submenu)
PopupMenu LabeledRotarySlider::createCCSubmenu(int startCC, int endCC) {
    PopupMenu submenu;

    for (int cc = startCC; cc <= endCC && cc <= 127; ++cc) {
        int itemID = 1000 + (0x10000 + cc); // Use Tracktion's controller ID format
        submenu.addItem(itemID, formatCCName(cc));
    }

    return submenu;
}

// Create submenu for special controllers (RPNs, NRPNs, Channel Pressure)
PopupMenu LabeledRotarySlider::createSpecialControllersSubmenu() {
    PopupMenu submenu;

    // Channel Pressure
    submenu.addItem(1000 + 0x40000, "Channel Pressure");
    submenu.addSeparator();

    // Common RPNs
    submenu.addItem(1000 + (0x30000 + 0), "RPN 0 (Pitch Bend Range)");
    submenu.addItem(1000 + (0x30000 + 1), "RPN 1 (Fine Tuning)");
    submenu.addItem(1000 + (0x30000 + 2), "RPN 2 (Coarse Tuning)");

    return submenu;
}

// Create the main MIDI mapping submenu with 9 submenus
PopupMenu LabeledRotarySlider::createMidiMappingSubmenu() {
    PopupMenu midiSubmenu;

    // 8 submenus for CC ranges (16 CCs each)
    for (int i = 0; i < 8; ++i) {
        int startCC = i * 16;
        int endCC = startCC + 15;

        String submenuName = "CC " + String(startCC) + "-" + String(endCC);
        PopupMenu ccSubmenu = createCCSubmenu(startCC, endCC);
        midiSubmenu.addSubMenu(submenuName, ccSubmenu);
    }

    midiSubmenu.addSeparator();

    // 9th submenu for special controllers
    PopupMenu specialSubmenu = createSpecialControllersSubmenu();
    midiSubmenu.addSubMenu("Special Controllers", specialSubmenu);

    return midiSubmenu;
}

//==============================================================================
// ChipInstrumentUI
//==============================================================================
ChipInstrumentUI::ChipInstrumentUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , instrument(instrumentPlugin()->instrument)
    , adsrAttackSlider  (instrument.oscParams.ampAttack)
    , adsrDecaySlider   (instrument.oscParams.ampDecay)
    , adsrSustainSlider (instrument.oscParams.ampSustain)
    , adsrReleaseSlider (instrument.oscParams.ampRelease)
    // , adsrVelocitySlider(instrument.oscParams.ampVelocity)
    , pitchAttackSlider  (instrument.oscParams.pitchAttack)
    , pitchDecaySlider   (instrument.oscParams.pitchDecay)
    , pitchSustainSlider (instrument.oscParams.pitchSustain)
    , pitchReleaseSlider (instrument.oscParams.pitchRelease)
    , pitchDepthSlider   (instrument.oscParams.pitchDepth)
{
    jassert(pluginPtr != nullptr);

    setSize(168, 320);

    addAndMakeVisible(adsrAttackSlider);
    addAndMakeVisible(adsrDecaySlider);
    addAndMakeVisible(adsrSustainSlider);
    addAndMakeVisible(adsrReleaseSlider);

    addAndMakeVisible(pitchAttackSlider);
    addAndMakeVisible(pitchDecaySlider);
    addAndMakeVisible(pitchSustainSlider);
    addAndMakeVisible(pitchReleaseSlider);
    addAndMakeVisible(pitchDepthSlider);
}

ChipInstrumentPlugin* ChipInstrumentUI::instrumentPlugin() {
    return dynamic_cast<ChipInstrumentPlugin*>(plugin.get());
}

void ChipInstrumentUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void ChipInstrumentUI::resized() {
    auto r = getLocalBounds().reduced(8);
    static constexpr int knobSize = 32;
    static constexpr int spacing = 8;

    // Amp ADSR row
    auto ampRow = r.removeFromTop(knobSize + adsrAttackSlider.getLabelHeight());
    adsrAttackSlider.setBounds(ampRow.removeFromLeft(knobSize));
    ampRow.removeFromLeft(spacing);
    adsrDecaySlider.setBounds(ampRow.removeFromLeft(knobSize));
    ampRow.removeFromLeft(spacing);
    adsrSustainSlider.setBounds(ampRow.removeFromLeft(knobSize));
    ampRow.removeFromLeft(spacing);
    adsrReleaseSlider.setBounds(ampRow.removeFromLeft(knobSize));

    r.removeFromTop(spacing); // Space between rows

    // Pitch ADSR row
    auto pitchRow = r.removeFromTop(knobSize + pitchAttackSlider.getLabelHeight());
    pitchAttackSlider.setBounds(pitchRow.removeFromLeft(knobSize));
    pitchRow.removeFromLeft(spacing);
    pitchDecaySlider.setBounds(pitchRow.removeFromLeft(knobSize));
    pitchRow.removeFromLeft(spacing);
    pitchSustainSlider.setBounds(pitchRow.removeFromLeft(knobSize));
    pitchRow.removeFromLeft(spacing);
    pitchReleaseSlider.setBounds(pitchRow.removeFromLeft(knobSize));

    r.removeFromTop(spacing); // Space between rows

    // Pitch Depth knob (separate row)
    auto depthRow = r.removeFromTop(knobSize + pitchDepthSlider.getLabelHeight());
    pitchDepthSlider.setBounds(depthRow.removeFromLeft(knobSize));
}

REGISTER_PLUGIN_UI_ADAPTER(ChipInstrumentPlugin, ChipInstrumentUI)

}  // namespace MoTool::uZX
