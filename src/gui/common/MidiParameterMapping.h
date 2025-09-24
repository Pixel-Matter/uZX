#pragma once

#include <JuceHeader.h>
#include <functional>

namespace MoTool {

namespace te = tracktion;

class MidiParameterMapping {
public:
    explicit MidiParameterMapping(te::AutomatableParameter::Ptr param);

    // Core mapping functionality
    void learnMidiCC();
    void clearMapping();
    void mapToMidiCC(int controllerID, int channel = 1);
    bool isParameterMapped() const;
    String getMappingDescription() const;

    // Menu creation
    void showMappingMenu(std::function<void()> onMappingChanged = nullptr);
    static PopupMenu addMidiMappingSubmenusToMenu(PopupMenu& menu);

    // Static utility functions for MIDI controller formatting
    static String formatCCName(int ccNumber);
    static String formatControllerDescription(int controllerID, int channel);
    static PopupMenu createCCSubmenu(int startCC, int endCC);
    static PopupMenu createSpecialControllersSubmenu();

private:
    te::AutomatableParameter::Ptr parameter;
    std::function<void()> mappingChangedCallback;

    void handleMenuResult(int result);
};

}  // namespace MoTool