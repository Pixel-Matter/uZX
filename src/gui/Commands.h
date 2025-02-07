#pragma once

#include <JuceHeader.h>

namespace MoTool::Commands {

//==============================================================================
/**  Application commans and menus

    == TODO ==
    - Make it nice declarative DSL-like class
*/

struct MenuItem {
    ApplicationCommandInfo info;
};


class AppCommands {
public:
    enum CommandIDs {
        // File Menu
        fileNew             = 1,
        fileOpen,
        fileSave,
        fileSaveAs,
        fileReveal,
        fileQuit,

        // Edit Menu
        editUndo            = 100,
        editRedo,
        editCut,
        editCopy,
        editPaste,

        // Transport Menu
        transportPlay       = 200,
        transportRecord,
        transportRewind,
        transportLoop,

        // Settings Menu
        settingsAudioMidi   = 300,
        settingsPlugins,

        // Help Menu
        helpAbout           = 1000

        // Add more commands as needed...
    };

    static StringArray getMenuBarNames() {
        return { "File", "Edit", "Transport", "Help" };
    }

    // Get all commands
    static Array<CommandID> getCommandIDs() {
        CommandID ids[] = {
            fileNew, fileOpen, fileSave, fileSaveAs, fileReveal, fileQuit,
            editUndo, editRedo, editCut, editCopy, editPaste,
            transportPlay, transportRecord, transportRewind, transportLoop,
            settingsAudioMidi, settingsPlugins,
            helpAbout
        };

        return Array<CommandID>(ids, numElementsInArray(ids));
    }

    static PopupMenu createMenu(ApplicationCommandManager* manager, const String& menuName) {
        PopupMenu menu;
        if (menuName == "File") {
            menu.addCommandItem(manager, AppCommands::fileNew);
            menu.addCommandItem(manager, AppCommands::fileOpen);
            menu.addSeparator();
            menu.addCommandItem(manager, AppCommands::fileSave);
            menu.addCommandItem(manager, AppCommands::fileSaveAs);
            menu.addCommandItem(manager, AppCommands::fileReveal);
            menu.addSeparator();
            menu.addCommandItem(manager, AppCommands::fileQuit);
        } else if (menuName == "Edit") {
            menu.addCommandItem(manager, AppCommands::editUndo);
            menu.addCommandItem(manager, AppCommands::editRedo);
            menu.addSeparator();
            menu.addCommandItem(manager, AppCommands::editCut);
            menu.addCommandItem(manager, AppCommands::editCopy);
            menu.addCommandItem(manager, AppCommands::editPaste);
        } else if (menuName == "Transport") {
            menu.addCommandItem(manager, AppCommands::transportPlay);
            menu.addCommandItem(manager, AppCommands::transportRecord);
            menu.addCommandItem(manager, AppCommands::transportRewind);
            menu.addSeparator();
            menu.addCommandItem(manager, AppCommands::transportLoop);
        } else if (menuName == "Settings") {
            menu.addCommandItem(manager, AppCommands::settingsAudioMidi);
            menu.addCommandItem(manager, AppCommands::settingsPlugins);
        } else if (menuName == "Help") {
            menu.addCommandItem(manager, AppCommands::helpAbout);
        }
        return menu;
    }

    static void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
        switch (commandID) {
            // File commands
            case fileNew:
                result.setInfo("New", "Create a new edit", "File", 0);
                result.addDefaultKeypress('n', ModifierKeys::commandModifier);
                break;

            case fileOpen:
                result.setInfo("Open...", "Open an existing edit", "File", 0);
                result.addDefaultKeypress('o', ModifierKeys::commandModifier);
                break;

            case fileSave:
                result.setInfo("Save", "Save the current edit", "File", 0);
                result.addDefaultKeypress('s', ModifierKeys::commandModifier);
                break;

            case fileSaveAs:
                result.setInfo("Save As...", "Save the current edit with a new name", "File", 0);
                result.addDefaultKeypress('s', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
                break;

            case fileReveal:
                result.setInfo("Reveal", "Reveal the current edit in Finder", "File", 0);
                break;

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

            case editCut:
                result.setInfo("Cut", "Cut the selected content", "Edit", 0);
                result.addDefaultKeypress('x', ModifierKeys::commandModifier);
                break;

            case editCopy:
                result.setInfo("Copy", "Copy the selected content", "Edit", 0);
                result.addDefaultKeypress('c', ModifierKeys::commandModifier);
                break;

            case editPaste:
                result.setInfo("Paste", "Paste the clipboard content", "Edit", 0);
                result.addDefaultKeypress('v', ModifierKeys::commandModifier);
                break;

            // Transport commands
            case transportPlay:
                result.setInfo("Play/Stop", "Start or stop playback", "Transport", 0);
                result.addDefaultKeypress(KeyPress::spaceKey, 0);
                break;

            case transportRecord:
                result.setInfo("Record", "Start or stop recording", "Transport", 0);
                result.addDefaultKeypress('r', 0);
                break;

            case transportRewind:
                result.setInfo("Rewind", "Rewind to the beginning", "Transport", 0);
                result.addDefaultKeypress(KeyPress::leftKey, ModifierKeys::commandModifier);
                break;

            case transportLoop:
                result.setInfo("Loop", "Toggle loop playback", "Transport", 0);
                result.addDefaultKeypress('l', ModifierKeys::commandModifier);
                break;

            // Settings commands
            case settingsAudioMidi:
                result.setInfo("Audio/MIDI", "Open audio and MIDI settings", "Settings", 0);
                break;

            case settingsPlugins:
                result.setInfo("Plugins", "Open plugin manager", "Settings", 0);
                break;

            // Help commands
            case helpAbout:
                result.setInfo("About", "Show about dialog", "Help", 0);
                break;

            // Add more command info...
        }
    }
};


class CommandManager  : public ApplicationCommandManager {
public:
    CommandManager() = default;

    void initializeWithTarget(ApplicationCommandTarget* target) {
        setFirstCommandTarget(target);
        registerAllCommandsForTarget();
    }

    void registerAllCommandsForTarget() {
        auto commands = AppCommands::getCommandIDs();
        for (auto commandID : commands) {
            ApplicationCommandInfo info(commandID);
            AppCommands::getCommandInfo(commandID, info);
            registerCommand(info);
        }
    }

    PopupMenu createMenu(const String& menuName) {
        return AppCommands::createMenu(this, menuName);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommandManager)
};

} // namespace MoTool::Commands
