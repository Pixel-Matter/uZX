#include "MidiParameterMapping.h"

namespace MoTool {

namespace te = tracktion;

MidiParameterMapping::MidiParameterMapping(te::AutomatableParameter::Ptr param)
    : parameter(param)
{
}

void MidiParameterMapping::learnMidiCC() {
    if (!parameter) return;
    // TODO make it really work

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

void MidiParameterMapping::clearMapping() {
    if (!parameter) return;

    auto& edit = parameter->getEdit();
    auto& mappings = edit.getParameterControlMappings();
    mappings.removeParameterMapping(*parameter);
}

bool MidiParameterMapping::isParameterMapped() const {
    if (!parameter) return false;

    auto& edit = parameter->getEdit();
    auto& mappings = edit.getParameterControlMappings();
    return mappings.isParameterMapped(*parameter);
}

String MidiParameterMapping::getMappingDescription() const {
    if (!parameter) return {};

    auto& edit = parameter->getEdit();
    auto& mappings = edit.getParameterControlMappings();

    int channel, controllerID;
    if (mappings.getParameterMapping(*parameter, channel, controllerID)) {
        return formatControllerDescription(controllerID, channel);
    }

    return "Unknown";
}

void MidiParameterMapping::mapToMidiCC(int controllerID, int channel) {
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
}

void MidiParameterMapping::showMappingMenu(std::function<void()> onMappingChanged) {
    if (!parameter) return;

    mappingChangedCallback = onMappingChanged;

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

    // FIXME not working
    menu.addItem(2, "Learn...");
    menu.addSeparator();

    // Add MIDI mapping options directly to the main menu
    addMidiMappingSubmenusToMenu(menu);

    auto result = menu.show();
    handleMenuResult(result);
}

void MidiParameterMapping::handleMenuResult(int result) {
    if (result == 1) {
        clearMapping();
        if (mappingChangedCallback) mappingChangedCallback();
    } else if (result == 2) {
        learnMidiCC();
    } else if (result >= 1000) {
        // Handle direct MIDI CC mapping (result IDs start from 1000)
        int controllerID = result - 1000;
        mapToMidiCC(controllerID);
        if (mappingChangedCallback) mappingChangedCallback();
    }
}

// Helper function to format CC names using JUCE's built-in names
String MidiParameterMapping::formatCCName(int ccNumber) {
    auto name = String(MidiMessage::getControllerName(ccNumber));
    String result = "CC " + String(ccNumber);

    if (name.isNotEmpty()) {
        result += " " + name;
    }

    return result;
}

// Standalone function to decode and format controller descriptions
String MidiParameterMapping::formatControllerDescription(int controllerID, int channel) {
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
PopupMenu MidiParameterMapping::createCCSubmenu(int startCC, int endCC) {
    PopupMenu submenu;

    for (int cc = startCC; cc <= endCC && cc <= 127; ++cc) {
        int itemID = 1000 + (0x10000 + cc); // Use Tracktion's controller ID format
        submenu.addItem(itemID, formatCCName(cc));
    }

    return submenu;
}

// Create submenu for special controllers (RPNs, NRPNs, Channel Pressure)
PopupMenu MidiParameterMapping::createSpecialControllersSubmenu() {
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
PopupMenu MidiParameterMapping::addMidiMappingSubmenusToMenu(PopupMenu& menu) {
    // 8 submenus for CC ranges (16 CCs each)
    for (int i = 0; i < 8; ++i) {
        int startCC = i * 16;
        int endCC = startCC + 15;

        String submenuName = "CC " + String(startCC) + "-" + String(endCC);
        PopupMenu ccSubmenu = createCCSubmenu(startCC, endCC);
        menu.addSubMenu(submenuName, ccSubmenu);
    }

    menu.addSeparator();

    // 9th submenu for special controllers
    PopupMenu specialSubmenu = createSpecialControllersSubmenu();
    menu.addSubMenu("Special Controllers", specialSubmenu);

    return menu;
}

}  // namespace MoTool