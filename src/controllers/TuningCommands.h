#pragma once

#include <JuceHeader.h>

namespace MoTool::Commands {

//==============================================================================
/**  Application commands and menus for Tuning Controller
*/

class TuningsAppCommands {
public:
    enum CommandIDs {
        // File Menu
        fileQuit  = 1,

        // Edit Menu
        editUndo             = 100,
        editRedo,

        // Settings Menu
        settingsAudioMidi    = 600,
        settingsPlugins,

        // Add more commands as needed...
    };

    static StringArray getMenuBarNames() {
        return { "File", "Edit", "Settings" };
    }

    // Get all commands
    static Array<CommandID> getCommandIDs() {
        CommandID ids[] = {
            fileQuit,
            editUndo, editRedo,
            settingsAudioMidi, settingsPlugins,
        };

        return Array<CommandID>(ids, numElementsInArray(ids));
    }

    static PopupMenu createMenu(ApplicationCommandManager* manager, const String& menuName) {
        PopupMenu menu;
        if (menuName == "File") {
            menu.addCommandItem(manager, fileQuit);
        } else if (menuName == "Edit") {
            menu.addCommandItem(manager, editUndo);
            menu.addCommandItem(manager, editRedo);
        } else if (menuName == "Settings") {
            menu.addCommandItem(manager, settingsAudioMidi);
            menu.addCommandItem(manager, settingsPlugins);
        }
        return menu;
    }

    static void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
        switch (commandID) {
            // File commands
            case fileQuit:
                result.setInfo("Quit", "Quit the application", "File", 0);
                result.addDefaultKeypress('q', ModifierKeys::commandModifier);
                break;

            // Edit commands
            case editUndo:
                result.setInfo("Undo", "Undo the last action", "Edit", 0);
                result.addDefaultKeypress('z', ModifierKeys::commandModifier);
                break;

            case editRedo:
                result.setInfo("Redo", "Redo the last action", "Edit", 0);
                result.addDefaultKeypress('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
                break;

            // Settings commands
            case settingsAudioMidi:
                result.setInfo("Audio/MIDI", "Open audio and MIDI settings", "Settings", 0);
                result.addDefaultKeypress(',', ModifierKeys::commandModifier);
                break;

            case settingsPlugins:
                result.setInfo("Plugins", "Open plugin manager", "Settings", 0);
                break;
        }
    }
};

} // namespace MoTool::Commands