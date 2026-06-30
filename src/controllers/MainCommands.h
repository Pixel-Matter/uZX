#pragma once

#include <JuceHeader.h>

namespace MoTool::Commands {

//==============================================================================
/**  Application commands and menus
*/

class MainAppCommands {
public:
    enum CommandIDs {
        // File Menu
        fileNew             = 1,
        fileOpen,
        fileOpenRecent1     = 10,
        fileOpenRecent2,
        fileOpenRecent3,
        fileOpenRecent4,
        fileOpenRecent5,
        fileOpenRecent6,
        fileOpenRecent7,
        fileOpenRecent8,
        fileClearRecentFiles = 19,
        fileSave            = 20,
        fileSaveAs,
        fileReveal,
        fileImportPsg,
        fileImportAudio,
        fileQuit,

        // Edit Menu
        editUndo             = 100,
        editRedo,
        editDelete,
        editCut,
        editCopy,
        editPaste,

        // Transport Menu
        transportPlay        = 200,
        transportRecord,
        transportRecordStop,
        transportToStart,
        transportToEnd,
        transportLoop,

        // Add Menu
        addAudioTrack        = 300,
        // addAutomationTrack,
        // addMidiClip,

        // Track menu
        trackRenderToAudio   = 400,

        // View menu
        viewZoomToProject    = 500,
        viewZoomToSelection,
        viewZoomIn,
        viewZoomOut,

        // Settings Menu
        settingsAudioMidi    = 600,
        settingsPlugins,
        settingsTuningTables, // Open Tuning Tables window

        // Help Menu
        helpAbout            = 1000

        // Add more commands as needed...
    };

    static StringArray getMenuBarNames() {
        return { "File", "Edit", "Transport", "Add", "Track", "View", "Settings", "Help" };
    }

    // Get all commands
    static Array<CommandID> getCommandIDs() {
        CommandID ids[] = {
            fileNew, fileOpen,
            fileOpenRecent1, fileOpenRecent2, fileOpenRecent3, fileOpenRecent4,
            fileOpenRecent5, fileOpenRecent6, fileOpenRecent7, fileOpenRecent8,
            fileClearRecentFiles,
            fileSave, fileSaveAs, fileReveal, fileImportPsg, fileImportAudio, fileQuit,
            editUndo, editRedo, editDelete, editCut, editCopy, editPaste,
            transportPlay, transportRecord, transportRecordStop, transportToStart, transportToEnd, transportLoop,
            addAudioTrack,
            // addAutomationTrack,
            trackRenderToAudio,
            viewZoomToProject, viewZoomToSelection, viewZoomIn, viewZoomOut,
            settingsAudioMidi, settingsPlugins, settingsTuningTables,
            helpAbout
        };

        return Array<CommandID>(ids, numElementsInArray(ids));
    }

    static PopupMenu createMenu(ApplicationCommandManager* manager, const String& menuName) {
        PopupMenu menu;
        if (menuName == "File") {
            menu.addCommandItem(manager, MainAppCommands::fileNew);
            menu.addCommandItem(manager, MainAppCommands::fileOpen);

            // Recent files submenu
            PopupMenu recentMenu;
            recentMenu.addCommandItem(manager, MainAppCommands::fileOpenRecent1);
            recentMenu.addCommandItem(manager, MainAppCommands::fileOpenRecent2);
            recentMenu.addCommandItem(manager, MainAppCommands::fileOpenRecent3);
            recentMenu.addCommandItem(manager, MainAppCommands::fileOpenRecent4);
            recentMenu.addCommandItem(manager, MainAppCommands::fileOpenRecent5);
            recentMenu.addCommandItem(manager, MainAppCommands::fileOpenRecent6);
            recentMenu.addCommandItem(manager, MainAppCommands::fileOpenRecent7);
            recentMenu.addCommandItem(manager, MainAppCommands::fileOpenRecent8);
            recentMenu.addSeparator();
            recentMenu.addCommandItem(manager, MainAppCommands::fileClearRecentFiles);
            menu.addSubMenu("Open Recent", recentMenu);

            menu.addSeparator();
            menu.addCommandItem(manager, MainAppCommands::fileSave);
            menu.addCommandItem(manager, MainAppCommands::fileSaveAs);
            menu.addCommandItem(manager, MainAppCommands::fileReveal);
            menu.addSeparator();
            menu.addCommandItem(manager, MainAppCommands::fileImportPsg);
            menu.addCommandItem(manager, MainAppCommands::fileImportAudio);
            menu.addSeparator();
            menu.addCommandItem(manager, MainAppCommands::settingsTuningTables); // Add Tuning Tool menu item
            menu.addSeparator();
            menu.addCommandItem(manager, MainAppCommands::fileQuit);
        } else if (menuName == "Edit") {
            menu.addCommandItem(manager, MainAppCommands::editUndo);
            menu.addCommandItem(manager, MainAppCommands::editRedo);
            menu.addSeparator();
            menu.addCommandItem(manager, MainAppCommands::editDelete);
            menu.addCommandItem(manager, MainAppCommands::editCut);
            menu.addCommandItem(manager, MainAppCommands::editCopy);
            menu.addCommandItem(manager, MainAppCommands::editPaste);
        } else if (menuName == "Transport") {
            menu.addCommandItem(manager, MainAppCommands::transportPlay);
            menu.addCommandItem(manager, MainAppCommands::transportRecord);
            menu.addCommandItem(manager, MainAppCommands::transportRecordStop);
            menu.addCommandItem(manager, MainAppCommands::transportToStart);
            menu.addCommandItem(manager, MainAppCommands::transportToEnd);
            menu.addSeparator();
            menu.addCommandItem(manager, MainAppCommands::transportLoop);
        } else if (menuName == "Add") {
            menu.addCommandItem(manager, MainAppCommands::addAudioTrack);
            // menu.addCommandItem(manager, AppCommands::addAutomationTrack);
        } else if (menuName == "Track") {
            menu.addCommandItem(manager, MainAppCommands::trackRenderToAudio);
        } else if (menuName == "View") {
            menu.addCommandItem(manager, MainAppCommands::viewZoomToProject);
            menu.addCommandItem(manager, MainAppCommands::viewZoomToSelection);
            menu.addCommandItem(manager, MainAppCommands::viewZoomIn);
            menu.addCommandItem(manager, MainAppCommands::viewZoomOut);
        } else if (menuName == "Settings") {
            menu.addCommandItem(manager, MainAppCommands::settingsAudioMidi);
            menu.addCommandItem(manager, MainAppCommands::settingsPlugins);
            menu.addCommandItem(manager, MainAppCommands::settingsTuningTables);
        } else if (menuName == "Help") {
            menu.addCommandItem(manager, MainAppCommands::helpAbout);
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

            case fileOpenRecent1:
                result.setInfo("Recent File 1", "Open recent file 1", "File", 0);
                break;

            case fileOpenRecent2:
                result.setInfo("Recent File 2", "Open recent file 2", "File", 0);
                break;

            case fileOpenRecent3:
                result.setInfo("Recent File 3", "Open recent file 3", "File", 0);
                break;

            case fileOpenRecent4:
                result.setInfo("Recent File 4", "Open recent file 4", "File", 0);
                break;

            case fileOpenRecent5:
                result.setInfo("Recent File 5", "Open recent file 5", "File", 0);
                break;

            case fileOpenRecent6:
                result.setInfo("Recent File 6", "Open recent file 6", "File", 0);
                break;

            case fileOpenRecent7:
                result.setInfo("Recent File 7", "Open recent file 7", "File", 0);
                break;

            case fileOpenRecent8:
                result.setInfo("Recent File 8", "Open recent file 8", "File", 0);
                break;

            case fileClearRecentFiles:
                result.setInfo("Clear Recent Files", "Clear recent files list", "File", 0);
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

            case fileImportPsg:
                result.setInfo("Import PSG...", "Import PSG file", "File", 0);
                result.addDefaultKeypress('i', ModifierKeys::commandModifier);
                break;

            case fileImportAudio:
                result.setInfo("Import Audio...", "Import audio file", "File", 0);
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

            // Add commands
            case addAudioTrack:
                result.setInfo("Audio Track", "Add a new audio track", "Add", 0);
                result.addDefaultKeypress('t', ModifierKeys::commandModifier);
                break;

            // case addAutomationTrack:
            //     result.setInfo("Automation Track", "Add a new automation track", "Add", 0);
            //     break;

            // Track commands
            case trackRenderToAudio:
                result.setInfo("Render to audio", "Render track to audio track", "Track", 0);
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

            case settingsTuningTables:
                result.setInfo("Tuning Tables", "Open the Tuning tables editor in a new window", "Settings", 0);
                result.addDefaultKeypress('T', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
                break;

            // Help commands
            case helpAbout:
                result.setInfo("About", "Show about dialog", "Help", 0);
                break;

            // Add more command info...
        }
    }
};


} // namespace MoTool::Commands
