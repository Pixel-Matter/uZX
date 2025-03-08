#pragma once

#include <JuceHeader.h>

namespace MoTool::Commands {

//==============================================================================
/**  Application commands and menus
*/

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
        editDelete,
        editCut,
        editCopy,
        editPaste,

        // Transport Menu
        transportPlay       = 200,
        transportRecord,
        transportRecordStop,
        transportToStart,
        transportToEnd,
        transportLoop,

        // View menu
        viewZoomToProject    = 300,
        viewZoomToSelection,
        viewZoomIn,
        viewZoomOut,

        // Settings Menu
        settingsAudioMidi   = 400,
        settingsPlugins,

        // Help Menu
        helpAbout           = 1000

        // Add more commands as needed...
    };

    static StringArray getMenuBarNames() {
        return { "File", "Edit", "Transport", "View", "Settings", "Help" };
    }

    // Get all commands
    static Array<CommandID> getCommandIDs() {
        CommandID ids[] = {
            fileNew, fileOpen, fileSave, fileSaveAs, fileReveal, fileQuit,
            editUndo, editRedo, editDelete, editCut, editCopy, editPaste,
            transportPlay, transportRecord, transportRecordStop, transportToStart, transportToEnd, transportLoop,
            viewZoomToProject, viewZoomToSelection, viewZoomIn, viewZoomOut,
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
            menu.addCommandItem(manager, AppCommands::editDelete);
            menu.addCommandItem(manager, AppCommands::editCut);
            menu.addCommandItem(manager, AppCommands::editCopy);
            menu.addCommandItem(manager, AppCommands::editPaste);
        } else if (menuName == "Transport") {
            menu.addCommandItem(manager, AppCommands::transportPlay);
            menu.addCommandItem(manager, AppCommands::transportRecord);
            menu.addCommandItem(manager, AppCommands::transportRecordStop);
            menu.addCommandItem(manager, AppCommands::transportToStart);
            menu.addCommandItem(manager, AppCommands::transportToEnd);
            menu.addSeparator();
            menu.addCommandItem(manager, AppCommands::transportLoop);
        } else if (menuName == "View") {
            menu.addCommandItem(manager, AppCommands::viewZoomToProject);
            menu.addCommandItem(manager, AppCommands::viewZoomToSelection);
            menu.addCommandItem(manager, AppCommands::viewZoomIn);
            menu.addCommandItem(manager, AppCommands::viewZoomOut);
        } else if (menuName == "Settings") {
            menu.addCommandItem(manager, AppCommands::settingsAudioMidi);
            menu.addCommandItem(manager, AppCommands::settingsPlugins);
        } else if (menuName == "Help") {
            menu.addCommandItem(manager, AppCommands::helpAbout);
        }
        return menu;
    }

    static void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
        // DBG("Appcommands::getCommandInfo: " << commandID);
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
                result.setInfo("Reveal in Finder", "Reveal the current edit in Finder", "File", 0);
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

            case editDelete:
                result.setInfo("Delete", "Delete the selected item", "Edit", 0);
                result.addDefaultKeypress(KeyPress::deleteKey, 0);
                result.addDefaultKeypress(KeyPress::backspaceKey, 0);
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
                result.setInfo("Record", "Start recording", "Transport", 0);
                // result.addDefaultKeypress('r', 0);
                break;

            case transportRecordStop:
                result.setInfo("Record stop", "Stop recording", "Transport", 0);
                // result.addDefaultKeypress('r', 0);
                break;

            case transportToStart:
                result.setInfo("Rewind", "Rewind to the beginning", "Transport", 0);
                result.addDefaultKeypress(KeyPress::leftKey, ModifierKeys::commandModifier);
                result.addDefaultKeypress(KeyPress::homeKey, 0);
                break;

            case transportToEnd:
                result.setInfo("End", "Go to the end", "Transport", 0);
                result.addDefaultKeypress(KeyPress::rightKey, ModifierKeys::commandModifier);
                result.addDefaultKeypress(KeyPress::endKey, 0);
                break;

            case transportLoop:
                result.setInfo("Loop", "Toggle loop playback", "Transport", 0);
                result.addDefaultKeypress('l', ModifierKeys::commandModifier);
                break;

            // View commands
            case viewZoomToProject:
                result.setInfo("Zoom to project", "Zoom to project", "View", 0);
                result.addDefaultKeypress('Z', ModifierKeys::shiftModifier);
                break;

            case viewZoomToSelection:
                result.setInfo("Zoom to selection", "Zoom to selection", "View", 0);
                result.addDefaultKeypress('Z', 0);
                break;

            case viewZoomIn:
                result.setInfo("Zoom in", "Zoom in", "View", 0);
                result.addDefaultKeypress('=', 0);
                break;

            case viewZoomOut:
                result.setInfo("Zoom out", "Zoom out", "View", 0);
                result.addDefaultKeypress('-', 0);
                break;

            // Settings commands
            case settingsAudioMidi:
                result.setInfo("Audio/MIDI", "Open audio and MIDI settings", "Settings", 0);
                result.addDefaultKeypress(',', ModifierKeys::commandModifier);
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

//==============================================================================
/**
    One of these objects holds a list of all the commands your app can perform,
    and despatches these commands when needed.

    Application commands are a good way to trigger actions in your app, e.g. "Quit",
    "Copy", "Paste", etc. Menus, buttons and keypresses can all be given commands
    to invoke automatically, which means you don't have to handle the result of a menu
    or button click manually. Commands are despatched to ApplicationCommandTarget objects
    which can choose which events they want to handle.

    This architecture also allows for nested ApplicationCommandTargets, so that for example
    you could have two different objects, one inside the other, both of which can respond to
    a "delete" command. Depending on which one has focus, the command will be sent to the
    appropriate place, regardless of whether it was triggered by a menu, keypress or some other
    method.
*/
// class CommandManager : public ApplicationCommandManager {
// public:
//     CommandManager() = default;

//     void initializeWithTarget(ApplicationCommandTarget* target) {
//         setFirstCommandTarget(target);
//         myRegisterAllCommandsForTarget();
//     }

//     void myRegisterAllCommandsForTarget() {
//         auto commands = AppCommands::getCommandIDs();
//         for (auto commandID : commands) {
//             ApplicationCommandInfo info(commandID);
//             AppCommands::getCommandInfo(commandID, info);
//             registerCommand(info);
//         }
//     }

//     PopupMenu createMenu(const String& menuName) {
//         return AppCommands::createMenu(this, menuName);
//     }

// private:
//     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommandManager)
// };


} // namespace MoTool::Commands
