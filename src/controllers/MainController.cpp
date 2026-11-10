#include <JuceHeader.h>
#include <memory>

#include "MainController.h"
#include "EditState.h"
#include "MainCommands.h"
#include "UIBehavior.h"

#include "../gui/main/MainWindow.h"
#include "../gui/main/MainDocument.h"
#include "../gui/common/Utilities.h"
#include "../gui/main/AboutDialog.h"
#include "../gui/timeline/PsgClipComponent.h"
#include "../gui/video/RenderVideoDialog.h"

#include "../models/Behavior.h"
#include "../models/EditUtilities.h"
#include "../models/Ids.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../plugins/uZX/instrument/ChipInstrumentPlugin.h"
#include "../plugins/uZX/notes_to_psg/NotesToPsgPlugin.h"
#include "../plugins/uZX/midi_logger/MidiLoggerPlugin.h"
#include "../plugins/uZX/scope/ScopePlugin.h"

#include "../util/FileOps.h"
#include "../util/Helpers.h"
#include "controllers/App.h"
#include "controllers/TuningController.h"

#include <common/Utilities.h>  // from Tracktion

using namespace MoTool::Commands;
using namespace MoTool::Helpers;

namespace MoTool {

namespace {
/** View > Viz Options: what Viz mode, and a rendered video, show and in which colours. The Player
    shows a change as it is saved. */
PopupMenu buildVizOptionsMenu(ApplicationCommandManager& commands, PropertiesFile& settings) {
    const auto current = VizOptions::load(settings);
    auto choose = [&settings](std::function<void(VizOptions&)> change) {
        return [&settings, change] {
            auto options = VizOptions::load(settings);
            change(options);
            options.save(settings);
        };
    };

    PopupMenu menu;
    for (int i = 0; i < VizOptions::layoutNames.size(); ++i) {
        const auto layout = (VizOptions::Layout) i;
        menu.addItem(VizOptions::layoutNames[i], true, current.layout == layout,
                     choose([layout](VizOptions& o) { o.layout = layout; }));
    }
    menu.addCommandItem(&commands, MainAppCommands::viewNextVizLayout);
    menu.addSeparator();
    for (int i = 0; i < VizOptions::scopesNames.size(); ++i) {
        const auto scopes = (VizOptions::Scopes) i;
        menu.addItem(VizOptions::scopesNames[i], true, current.scopes == scopes,
                     choose([scopes](VizOptions& o) { o.scopes = scopes; }));
    }
    menu.addCommandItem(&commands, MainAppCommands::viewSwitchVizScopes);
    menu.addSeparator();
    for (int i = 0; i < VizOptions::colourNames.size(); ++i) {
        const auto colours = (VizOptions::Colours) i;
        menu.addItem(VizOptions::colourNames[i], true, current.colours == colours,
                     choose([colours](VizOptions& o) { o.colours = colours; }));
    }
    return menu;
}
}  // namespace

void registerPlugins(te::Engine& engine) {
    engine.getPluginManager().createBuiltInType<uZX::AYChipPlugin>();
    engine.getPluginManager().createBuiltInType<uZX::ChipInstrumentPlugin>();
    engine.getPluginManager().createBuiltInType<uZX::NotesToPsgPlugin>();
    engine.getPluginManager().createBuiltInType<uZX::MidiLoggerPlugin>();
    engine.getPluginManager().createBuiltInType<uZX::ScopePlugin>();
}

AppController::AppController()
    : engine_ {
        std::make_unique<PropertyStorage>(MoToolApp::getApp().getApplicationName()),
        std::make_unique<ExtUIBehaviour>(),
        std::make_unique<ExtEngineBehaviour>()
    }
{
    // engine_.getDeviceManager().initialise(engine_.getEngineBehaviour().shouldOpenAudioInputByDefault()
    //     ? te::DeviceManager::defaultNumChannelsToOpen : 0, te::DeviceManager::defaultNumChannelsToOpen);

    // Workaround for JUCE CoreAudio buffer overflow bug at low sample rates
    ensureMinimumSampleRate();

    // A previously-saved setup may pair a separate input device with the output device, which
    // freezes playback on macOS. Drop the mismatched input before the user hits an unplayable state.
    sanitizeAudioInputDevice();
}

void AppController::sanitizeAudioInputDevice() {
    auto& deviceManager = engine_.getDeviceManager().deviceManager;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (setup.inputDeviceName.isNotEmpty() && setup.inputDeviceName != setup.outputDeviceName) {
        setup.inputDeviceName = {};
        setup.inputChannels.clear();
        setup.useDefaultInputChannels = true;
        deviceManager.setAudioDeviceSetup(setup, true);
    }
}

void AppController::initialize() {
    registerPlugins(engine_);

    engine_.getDeviceManager().addChangeListener(this);

    selectionManager_.addChangeListener(this);

    commandManager_.registerAllCommandsForTarget(this);
    commandManager_.setFirstCommandTarget(this);
    // Install key mappings
    commandManager_.getKeyMappings()->resetToDefaultMappings();

    // menuBarModel::
    setApplicationCommandManagerToWatch(&commandManager_);

    #if JUCE_MAC
    // macOS puts About and Settings in the app menu, next to the Quit item it adds itself
    PopupMenu appMenu;
    appMenu.addCommandItem(&commandManager_, MainAppCommands::helpAbout,
                           "About " + JUCEApplication::getInstance()->getApplicationName());
    appMenu.addSeparator();
    appMenu.addCommandItem(&commandManager_, MainAppCommands::settingsAudioMidi, "Settings...");
    setMacMainMenu(this, &appMenu);
    #else
    // Menu bar is set on the window in ArrangerController::initialize()
    #endif
}

AppController::~AppController() {
    engine_.getDeviceManager().removeChangeListener(this);
    engine_.getTemporaryFileManager().getTempDirectory().deleteRecursively();

    // commandManager_.setFirstCommandTarget(nullptr);
    selectionManager_.deselectAll();
    selectionManager_.removeChangeListener(this);

    commandManager_.setFirstCommandTarget(nullptr);
    setApplicationCommandManagerToWatch(nullptr);

    #if JUCE_MAC
        setMacMainMenu(nullptr);
    #else
    // Menu bar is cleared on the window in ArrangerController destructor
    #endif
}

te::SelectionManager& AppController::getSelectionManager() {
    return selectionManager_;
}

ApplicationCommandManager& AppController::getCommandManager() {
    return commandManager_;
}

void AppController::ensureMinimumSampleRate() {
    constexpr double MIN_SAMPLE_RATE = 44100.0;
    constexpr int MAX_BLOCK_SIZE = 256;

    auto& deviceManager = engine_.getDeviceManager().deviceManager;
    auto* currentDevice = deviceManager.getCurrentAudioDevice();

    bool audioOk = false;

    if (currentDevice != nullptr) {
        auto currentSampleRate = currentDevice->getCurrentSampleRate();
        if (currentSampleRate < MIN_SAMPLE_RATE) {
            const auto currentBufferSize = currentDevice->getCurrentBufferSizeSamples();
            DBG("WARNING: Sample rate " << currentSampleRate << " Hz is below minimum " << MIN_SAMPLE_RATE << " Hz"
                << ", buffer size: " << currentBufferSize);
            auto desiredBlockSize = juce::jmin(currentBufferSize, MAX_BLOCK_SIZE);

            auto availableSizes = currentDevice->getAvailableBufferSizes();
            if (!availableSizes.isEmpty()) {
                int candidateSize = availableSizes.getLast();
                for (auto size : availableSizes) {
                    if (size >= MAX_BLOCK_SIZE) {
                        candidateSize = size;
                        break;
                    }
                }
                desiredBlockSize = juce::jmin(currentBufferSize, candidateSize);
            }

            if (currentBufferSize > desiredBlockSize) {
                juce::AudioDeviceManager::AudioDeviceSetup setup;
                deviceManager.getAudioDeviceSetup(setup);
                setup.bufferSize = desiredBlockSize;
                DBG("  Current buffer size: " << currentBufferSize << ", setting to " << desiredBlockSize);
                if (auto error = deviceManager.setAudioDeviceSetup(setup, false); error.isNotEmpty())
                    DBG("Failed to set audio block size: " << error);
            }

            deviceManager.restartLastAudioDevice();
            const auto newBufferSize = currentDevice->getCurrentBufferSizeSamples();
            DBG("  New buffer size: " << newBufferSize);
        } else {
            audioOk = true;
        }
    }
    if (!audioOk) {
        MessageManager::callAsync([] {
            te::AppFunctions::showSettingsScreen();
        });
    }
}

String AppController::getAudioDeviceError(bool requireRunning) const {
    auto& deviceManager = engine_.getDeviceManager().deviceManager;
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return "No audio output device is selected.";

    if (!device->isOpen())
        return "The audio device \"" + device->getName() + "\" could not be opened.";

    if (auto lastError = device->getLastError(); lastError.isNotEmpty())
        return "Audio device \"" + device->getName() + "\" error: " + lastError;

    if (device->getActiveOutputChannels().isZero())
        return "The audio device \"" + device->getName() + "\" has no active output channels.";

    // The device callback not running is the symptom behind the frozen playhead (e.g. a failed
    // input device). Only treat it as fatal when we're about to start playback, since at startup
    // the device may not have spun up yet.
    if (requireRunning && !device->isPlaying())
        return "The audio device \"" + device->getName() + "\" is not running.";

    return {};
}

void AppController::reportAudioDeviceProblem(const String& error) {
    AlertWindow::showMessageBoxAsync(
        AlertWindow::WarningIcon,
        "Audio Problem",
        error + "\n\nPlayback will not advance until a working audio device is configured. "
                "Please check your audio settings.",
        "Open Audio Settings",
        nullptr,
        ModalCallbackFunction::create([](int) {
            te::AppFunctions::showSettingsScreen();
        }));
}

bool AppController::ensureAudioReadyForPlayback() {
    auto error = getAudioDeviceError(/* requireRunning */ true);
    if (error.isEmpty())
        return true;

    reportAudioDeviceProblem(error);
    return false;
}

void AppController::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &selectionManager_) {
        // Selection changed, update command status
        commandManager_.commandStatusChanged();
    }
    // Note: an invalid audio configuration is prevented at the source by AudioSettingsComponent,
    // and a leftover bad setup is caught by the play guard (ensureAudioReadyForPlayback).
}

void AppController::handlePluginManager() {
    DialogWindow::LaunchOptions o;
    o.dialogTitle                   = TRANS("Plugins");
    o.dialogBackgroundColour        = Colors::Theme::background;
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = true;
    o.useBottomRightCornerResizer   = true;

    auto v = new PluginListComponent (engine_.getPluginManager().pluginFormatManager,
                                      engine_.getPluginManager().knownPluginList,
                                      engine_.getTemporaryFileManager().getTempFile("PluginScanDeadMansPedal"),
                                      std::addressof(engine_.getPropertyStorage().getPropertiesFile()));
    v->setSize(800, 600);
    o.content.setOwned(v);
    Helpers::launchDialogAsync(o);
}

void AppController::showAboutDialog() {
    DialogWindow::LaunchOptions options;
    options.dialogTitle                  = "About " + JUCEApplication::getInstance()->getApplicationName();
    options.dialogBackgroundColour       = Colors::Theme::backgroundAlt;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = false;
    options.useBottomRightCornerResizer  = false;
    // TODO getCurrentFocusedDesktopWindow
    // options.componentToCentreAround      = &mainWindow_;
    options.content.setOwned(new AboutDialogComponent());
    Helpers::launchDialogAsync(options)->runModalLoop();
}


// ==============================================================================
// MenuBarModel
//==============================================================================
StringArray AppController::getMenuBarNames() {
    const bool isPlayer = MoToolApp::getTarget() == MoToolApp::Target::uZXPlayer;
    auto names = isPlayer ? StringArray { "File", "Edit", "Transport", "View", "Settings", "Help" }
                          : MainAppCommands::getMenuBarNames();
   #if JUCE_MAC
    // About and Settings are in the app menu, see initialize()
    names.removeString("Help");
    if (isPlayer)
        names.removeString("Settings");
   #endif
    return names;
}

PopupMenu AppController::getMenuForIndex(int /* menuIndex */, const String& menuName) {
    bool isPlayer = MoToolApp::getTarget() == MoToolApp::Target::uZXPlayer;

    if (menuName == "File") {
        PopupMenu menu;

        if (!isPlayer)
            menu.addCommandItem(&commandManager_, MainAppCommands::fileNew);
        menu.addCommandItem(&commandManager_, MainAppCommands::fileOpen);

        // Recent files submenu - populate dynamically
        auto recentFiles = EditFileOps::getRecentEdits(engine_.getPropertyStorage());
        if (recentFiles.size() > 0) {
            PopupMenu recentMenu;

            for (int i = 0; i < recentFiles.size() && i < 8; ++i) {
                File recentFile(recentFiles[i]);
                recentMenu.addCommandItem(&commandManager_, MainAppCommands::fileOpenRecent1 + i);
            }

            recentMenu.addSeparator();
            recentMenu.addCommandItem(&commandManager_, MainAppCommands::fileClearRecentFiles);
            menu.addSubMenu("Open Recent", recentMenu);
        }

        menu.addSeparator();

        menu.addCommandItem(&commandManager_, MainAppCommands::fileSave);
        menu.addCommandItem(&commandManager_, MainAppCommands::fileSaveAs);
        menu.addCommandItem(&commandManager_, MainAppCommands::fileReveal);
        menu.addSeparator();

        menu.addCommandItem(&commandManager_, MainAppCommands::fileImportPsg);
        if (!isPlayer)
            menu.addCommandItem(&commandManager_, MainAppCommands::fileImportAudio);
        menu.addSeparator();
        menu.addCommandItem(&commandManager_, MainAppCommands::fileRenderVideo);
       #if ! JUCE_MAC  // macOS has Quit in the app menu
        menu.addSeparator();
        menu.addCommandItem(&commandManager_, MainAppCommands::fileQuit);
       #endif

        return menu;
    }

    if (isPlayer) {
        // Reduced menu set for player
        PopupMenu menu;
        if (menuName == "Edit") {
            menu.addCommandItem(&commandManager_, MainAppCommands::editUndo);
            menu.addCommandItem(&commandManager_, MainAppCommands::editRedo);
        } else if (menuName == "Transport") {
            menu.addCommandItem(&commandManager_, MainAppCommands::transportPlay);
            menu.addCommandItem(&commandManager_, MainAppCommands::transportToStart);
            menu.addSeparator();
            menu.addCommandItem(&commandManager_, MainAppCommands::transportLoop);
        } else if (menuName == "View") {
            menu.addCommandItem(&commandManager_, MainAppCommands::viewZoomToProject);
            menu.addCommandItem(&commandManager_, MainAppCommands::viewZoomIn);
            menu.addCommandItem(&commandManager_, MainAppCommands::viewZoomOut);
            menu.addSeparator();
            menu.addCommandItem(&commandManager_, MainAppCommands::viewSnapPlayheadToGrid);
            menu.addCommandItem(&commandManager_, MainAppCommands::viewNoteLanes);
            menu.addCommandItem(&commandManager_, MainAppCommands::viewPianoKeys);
            menu.addSeparator();
            menu.addCommandItem(&commandManager_, MainAppCommands::viewVizMode);
            menu.addSubMenu("Viz Options",
                            buildVizOptionsMenu(commandManager_, engine_.getPropertyStorage().getPropertiesFile()));
            if (auto* edit = MoToolApp::getPlayerController().getEdit()) {
                menu.addSeparator();
                menu.addSubMenu("Timecode Format", Helpers::buildTimecodeFormatMenu(*edit));
            }
        } else if (menuName == "Settings") {
            menu.addCommandItem(&commandManager_, MainAppCommands::settingsAudioMidi);
        } else if (menuName == "Help") {
            menu.addCommandItem(&commandManager_, MainAppCommands::helpAbout);
        }
        return menu;
    }

    auto menu = MainAppCommands::createMenu(&commandManager_, menuName);

    if (menuName == "View") {
        if (auto* edit = MoToolApp::getArrangerController().getEdit()) {
            menu.addSeparator();
            menu.addSubMenu("Timecode Format", Helpers::buildTimecodeFormatMenu(*edit));
        }
    }

    return menu;
}

void AppController::menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) {
    // handled by CommandManager
}

ApplicationCommandTarget* AppController::getNextCommandTarget() {
    return nullptr;
}

void AppController::getAllCommands(Array<CommandID>& commands) {
    commands.addArray(MainAppCommands::getCommandIDs());
}

void AppController::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
    // DBG("MainWindow::getCommandInfo: " << commandID);
    // Get main command info, name, desc, keypresses
    MainAppCommands::getCommandInfo(commandID, result);

    bool isPlayer = MoToolApp::getTarget() == MoToolApp::Target::uZXPlayer;
    auto& controller = MoToolApp::getActiveController();
    auto* edit = controller.getEdit();

    // Update command status based on current state
    switch (commandID) {
        case MainAppCommands::fileSave:
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::fileSaveAs:
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::fileReveal:
            // the Player's opened PSG files live in the temp folder until saved
            result.setActive(edit != nullptr && (!isPlayer || !EditFileOps::isTempEdit(*edit)));
            break;

        case MainAppCommands::fileRenderVideo:
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::fileOpenRecent1:
        case MainAppCommands::fileOpenRecent2:
        case MainAppCommands::fileOpenRecent3:
        case MainAppCommands::fileOpenRecent4:
        case MainAppCommands::fileOpenRecent5:
        case MainAppCommands::fileOpenRecent6:
        case MainAppCommands::fileOpenRecent7:
        case MainAppCommands::fileOpenRecent8: {
            auto recentFiles = EditFileOps::getRecentEdits(engine_.getPropertyStorage());
            int fileIndex = commandID - MainAppCommands::fileOpenRecent1;
            if (fileIndex < recentFiles.size()) {
                File recentFile(recentFiles[fileIndex]);
                result.setInfo(recentFile.getFileNameWithoutExtension(),
                             "Open " + recentFile.getFullPathName(), "File", 0);
                result.setActive(true);
            } else {
                result.setActive(false);
            }
            break;
        }

        case MainAppCommands::fileClearRecentFiles: {
            auto recentFiles = EditFileOps::getRecentEdits(engine_.getPropertyStorage());
            result.setActive(recentFiles.size() > 0);
            break;
        }

        case MainAppCommands::editUndo:
            result.setActive(edit != nullptr && edit->getUndoManager().canUndo());
            break;

        case MainAppCommands::editRedo:
            result.setActive(edit != nullptr && edit->getUndoManager().canRedo());
            break;

        // the Player can't add tracks or chips back, and has no Tuning window
        case MainAppCommands::editDelete:
        case MainAppCommands::settingsTuningTables:
            result.setActive(!isPlayer);
            break;

        // TODO  Theese commands are not being updated on selection change, only transportPlay and transportRecordStop are updated
        // case AppCommands::editDelete:
        //     if (edit_ != nullptr) {
        //         if (auto sm = edit_->engine.getUIBehaviour().getCurrentlyFocusedSelectionManager()) {
        //             DBG("Selected objects: " << sm->getSelectedObjects().size());
        //             result.setActive(!sm->getSelectedObjects().isEmpty());
        //             break;
        //         }
        //     }
        //     result.setActive(false);
        //     break;

        case MainAppCommands::transportPlay:
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::transportRecord:
            result.setActive(!isPlayer && edit != nullptr && !edit->getTransport().isRecording());
            break;

        case MainAppCommands::transportRecordStop:
            result.setActive(!isPlayer && edit != nullptr && edit->getTransport().isRecording());
            break;

        case MainAppCommands::transportToStart:
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::transportLoop:
            result.setTicked(edit != nullptr && edit->getTransport().looping);
            break;

        case MainAppCommands::transportPreservePsgTiming:
            if (!isPlayer) {
                if (auto* editViewState = MoToolApp::getArrangerController().getEditViewState()) {
                    result.setActive(true);
                    result.setTicked(editViewState->shouldPreservePsgTimingOnTempoChange());
                    break;
                }
            }

            result.setActive(false);
            break;

        case MainAppCommands::viewZoomToSelection:
            result.setActive(edit != nullptr && selectionManager_.getSelectedObjects().size() > 0);
            break;

        case MainAppCommands::viewNoteLanes:
            result.setTicked(areNoteLanesShown(engine_));
            break;

        case MainAppCommands::viewPianoKeys:
            result.setActive(areNoteLanesShown(engine_));
            result.setTicked(arePianoKeysShown(engine_));
            break;

        case MainAppCommands::viewVizMode:
            result.setActive(isPlayer && edit != nullptr);
            result.setTicked(isPlayer && MoToolApp::getPlayerController().isVizMode());
            break;

        case MainAppCommands::viewExitVizMode:
        case MainAppCommands::viewNextVizLayout:
            result.setActive(isPlayer && MoToolApp::getPlayerController().isVizMode());
            break;

        case MainAppCommands::viewSwitchVizScopes:
            result.setActive(isPlayer && MoToolApp::getPlayerController().isVizMode()
                             && VizOptions::load(engine_.getPropertyStorage().getPropertiesFile()).showsScopes());
            break;

        case MainAppCommands::viewSnapPlayheadToGrid:
            if (auto* editViewState = controller.getEditViewState()) {
                result.setActive(true);
                result.setTicked(editViewState->shouldSnapPlayheadToGrid());
                break;
            }

            result.setActive(false);
            break;

        // Add more dynamic command states...
    }
}

bool AppController::perform(const InvocationInfo& info) {
    // DBG("MainWindow::perform: " << info.commandID);
    bool isPlayer = MoToolApp::getTarget() == MoToolApp::Target::uZXPlayer;

    switch (info.commandID) {
        case MainAppCommands::fileNew:
            if (!isPlayer) MoToolApp::getArrangerController().handleNew();
            break;

        case MainAppCommands::fileOpen:
            if (isPlayer)
                MoToolApp::getPlayerController().handleOpen();
            else
                MoToolApp::getArrangerController().handleOpen();
            break;

        case MainAppCommands::fileOpenRecent1:
        case MainAppCommands::fileOpenRecent2:
        case MainAppCommands::fileOpenRecent3:
        case MainAppCommands::fileOpenRecent4:
        case MainAppCommands::fileOpenRecent5:
        case MainAppCommands::fileOpenRecent6:
        case MainAppCommands::fileOpenRecent7:
        case MainAppCommands::fileOpenRecent8:
            if (isPlayer)
                MoToolApp::getPlayerController().handleOpenRecent(info.commandID - MainAppCommands::fileOpenRecent1);
            else
                MoToolApp::getArrangerController().handleOpenRecent(info.commandID - MainAppCommands::fileOpenRecent1);
            break;

        case MainAppCommands::fileClearRecentFiles:
            if (isPlayer)
                MoToolApp::getPlayerController().handleClearRecentFiles();
            else
                MoToolApp::getArrangerController().handleClearRecentFiles();
            break;

        case MainAppCommands::fileSave:
            if (isPlayer)
                MoToolApp::getPlayerController().handleSave();
            else
                te::AppFunctions::saveEdit();
            break;

        case MainAppCommands::fileSaveAs:
            if (isPlayer)
                MoToolApp::getPlayerController().handleSaveAs();
            else
                MoToolApp::getArrangerController().handleSaveAs();
            break;

        case MainAppCommands::fileReveal:
            if (auto* edit = MoToolApp::getActiveController().getEdit()) {
                EditFileOps::saveEdit(*edit, false, true, false);
                te::EditFileOperations(*edit).getEditFile().revealToUser();
            }
            break;

        case MainAppCommands::fileImportPsg:
            if (isPlayer) {
                MoToolApp::getPlayerController().handleImportPsgReplace();
            } else {
                auto* edit = MoToolApp::getArrangerController().getEdit();
                if (edit != nullptr) {
                    importPsgAsClip(*edit, selectionManager_);
                }
            }
            break;

        case MainAppCommands::fileImportAudio:
            if (!isPlayer) {
                auto* edit = MoToolApp::getArrangerController().getEdit();
                if (edit != nullptr) {
                    importAudioAsClip(*edit, selectionManager_);
                }
            }
            break;

        case MainAppCommands::fileRenderVideo:
            if (auto* edit = MoToolApp::getActiveController().getEdit())
                showRenderVideoDialog(*edit);
            break;

        case MainAppCommands::fileQuit:
            JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        case MainAppCommands::editUndo:
            te::AppFunctions::undo();
            break;

        case MainAppCommands::editRedo:
            te::AppFunctions::redo();
            break;

        case MainAppCommands::editDelete:
            te::AppFunctions::deleteSelected();
            break;
        // Transport commands
        case MainAppCommands::transportPlay: {
            // Guard against a misconfigured audio device: starting playback with a broken
            // device leaves the audible time frozen and the playhead stuck (see getAudioDeviceError).
            auto* activeEdit = isPlayer ? MoToolApp::getPlayerController().getEdit()
                                        : MoToolApp::getArrangerController().getEdit();
            const bool isPlaying = activeEdit != nullptr && activeEdit->getTransport().isPlaying();
            if (!isPlaying && !ensureAudioReadyForPlayback())
                break;
            if (!isPlaying && activeEdit != nullptr)
                PlayWholeEdit::rewindIfAtEnd(*activeEdit);
            te::AppFunctions::startStopPlay();
            break;
        }

        case MainAppCommands::transportRecord:
            if (!isPlayer) MoToolApp::getArrangerController().handleRecord();
            break;

        case MainAppCommands::transportRecordStop:
            if (!isPlayer) MoToolApp::getArrangerController().handleRecord();
            break;

        case MainAppCommands::transportToStart:
            te::AppFunctions::goToStart();
            break;

        case MainAppCommands::transportToEnd:
            te::AppFunctions::goToEnd();
            break;

        case MainAppCommands::transportLoop:
            te::AppFunctions::toggleLoop();
            break;

        case MainAppCommands::transportPreservePsgTiming:
            if (!isPlayer) {
                if (auto* editViewState = MoToolApp::getArrangerController().getEditViewState()) {
                    editViewState->setPreservePsgTimingOnTempoChange(
                        ! editViewState->shouldPreservePsgTimingOnTempoChange());
                    commandManager_.commandStatusChanged();
                }
            }
            break;

        // Add commands
        case MainAppCommands::addAudioTrack:
            if (!isPlayer) {
                auto* edit = MoToolApp::getArrangerController().getEdit();
                if (edit != nullptr) {
                    Helpers::addAndSelectAudioTrack(*edit, selectionManager_);
                }
            }
            break;

        // case AppCommands::addAutomationTrack:
        //     if (edit_ != nullptr) {
        //         Helpers::addAndSelectAutomationTrack(*edit_, selectionManager_);
        //     }
        //     break;

        // Track commands
        case MainAppCommands::trackRenderToAudio:
            if (!isPlayer) {
                auto* edit = MoToolApp::getArrangerController().getEdit();
                if (edit != nullptr) {
                    Helpers::renderSelectedTracksToAudioTrack(*edit, selectionManager_);
                }
            }
            break;

        // View commands
        case MainAppCommands::viewZoomToProject:
            // TODO maybe invert this, put implementation here
            // and te::AppFunctions::zoomToFitHorizontally() invokes this command
            // rationale: what is zoom and how to zoom must be defined by the current controller
            te::AppFunctions::zoomToFitHorizontally();
            break;

        case MainAppCommands::viewZoomToSelection:
            te::AppFunctions::zoomToSelection();
            break;

        case MainAppCommands::viewZoomIn:
            te::AppFunctions::zoomIn();
            break;

        case MainAppCommands::viewZoomOut:
            te::AppFunctions::zoomOut();
            break;

        case MainAppCommands::viewSnapPlayheadToGrid:
            if (auto* editViewState = MoToolApp::getActiveController().getEditViewState()) {
                editViewState->setSnapPlayheadToGrid(! editViewState->shouldSnapPlayheadToGrid());
                commandManager_.commandStatusChanged();
            }
            break;

        case MainAppCommands::viewNoteLanes:
            setNoteLanesShown(engine_, ! areNoteLanesShown(engine_));
            commandManager_.commandStatusChanged();
            break;

        case MainAppCommands::viewPianoKeys:
            setPianoKeysShown(engine_, ! arePianoKeysShown(engine_));
            commandManager_.commandStatusChanged();
            break;

        case MainAppCommands::viewVizMode:
        case MainAppCommands::viewExitVizMode:
            if (isPlayer) {
                auto& player = MoToolApp::getPlayerController();
                player.setVizMode(info.commandID == MainAppCommands::viewVizMode && ! player.isVizMode());
                commandManager_.commandStatusChanged();
            }
            break;

        case MainAppCommands::viewNextVizLayout: {
            auto& settings = engine_.getPropertyStorage().getPropertiesFile();
            auto options = VizOptions::load(settings);
            options.layout = options.nextLayout();
            options.save(settings);  // the Player shows it
            break;
        }

        case MainAppCommands::viewSwitchVizScopes: {
            auto& settings = engine_.getPropertyStorage().getPropertiesFile();
            auto options = VizOptions::load(settings);
            options.scopes = options.otherScopes();
            options.save(settings);  // the Player shows it
            break;
        }

        // Settings commands
        case MainAppCommands::settingsAudioMidi:
            te::AppFunctions::showSettingsScreen();
            break;

        case MainAppCommands::settingsPlugins:
            handlePluginManager();
            break;

        case MainAppCommands::settingsTuningTables: {
            // Open the Tuning Tool in a new window
            openTuningWindow();
            break;
        }

        case MainAppCommands::helpAbout:
            showAboutDialog();
            break;

        default:
            return false;
    }

    return true;
}

void AppController::openTuningWindow() {
    if (tuningController_ != nullptr) {
        tuningController_->bringWindowToFront();
    } else {
        tuningController_ = std::make_unique<TuningController>(getEngine());
        tuningController_->initialize();
    }
}

void AppController::closeTuningWindow() {
    tuningController_.reset();
}

te::Engine& AppController::getEngine() {
    return engine_;
}

//============================================================================
BaseController::BaseController(te::Engine& e)
    : engine_ {e}
{}

BaseController::~BaseController() {
    engine_.getDeviceManager().removeChangeListener(this);

    window_.clearContentComponent();

    if (edit_ != nullptr) {
        EditFileOps::saveEdit(*edit_, true, true, false);
        edit_->getTempDirectory(false).deleteRecursively();
    }
}

void BaseController::initialize() {
    engine_.getDeviceManager().addChangeListener(this);
    masterVolume_.addListener(this);

    auto edit = createOrLoadStartupEdit();
    if (edit == nullptr)  // the last project is unreadable: start with an empty one
        edit = createOrLoadEdit(EditFileOps::getTempEditFile());
    setEdit(std::move(edit));

    window_.restoreWindowBounds();
    window_.addKeyListener(MoToolApp::getCommandManager().getKeyMappings());
}

static const char* const masterVolumeDbKey = "masterVolumeDb";

void BaseController::useGlobalMasterVolume() {
    auto& storage = dynamic_cast<PropertyStorage&>(engine_.getPropertyStorage());
    auto masterVolume = edit_->getMasterVolumePlugin();
    masterVolume->setVolumeDb(static_cast<float>(storage.getCustomProperty(masterVolumeDbKey, 0.0)));
    masterVolume_.referTo(masterVolume->state.getPropertyAsValue(te::IDs::volume, nullptr));
}

void BaseController::valueChanged(Value&) {
    auto& storage = dynamic_cast<PropertyStorage&>(engine_.getPropertyStorage());
    storage.setCustomProperty(masterVolumeDbKey, edit_->getMasterVolumePlugin()->getVolumeDb());
}

void BaseController::bringWindowToFront() {
    window_.toFront(true);
    window_.setVisible(true);
}

void BaseController::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &engine_.getDeviceManager()) {
        devicesChanged();
    }
}

void BaseController::setMainWindowTitle(const String& title) {
    window_.setTitle(title);
    window_.setName(title);
}

te::Edit* BaseController::getEdit() {
    return edit_.get();
}

void BaseController::zoomHorizontal(float increment) {
    if (editViewState_ != nullptr)
        editViewState_->zoom.zoomHorizontally(increment);
}

void BaseController::zoomToFitHorizontally() {
    if (editViewState_ == nullptr)
        return;
    if (auto range = Helpers::getEffectiveClipsTimeRange(*edit_); !range.isEmpty())
        editViewState_->zoom.setRange(range);
}

te::Engine& BaseController::getEngine() {
    return engine_;
}

std::unique_ptr<te::Edit> BaseController::createOrLoadEdit(File editFile) {
    // Already open: a second copy would be saved over the first and lose its unsaved changes
    if (edit_ != nullptr && editFile == te::EditFileOperations(*edit_).getEditFile())
        return nullptr;
    if (!editFile.existsAsFile())
        return te::createEmptyEdit(engine_, editFile);

    auto edit = te::loadEditFromFile(engine_, editFile);
    if (edit == nullptr)
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
            "Can't Open Project",
            "'" + editFile.getFileName() + "' is not a " + String::fromUTF8("µZX") + " project or is damaged.",
            "OK");
    return edit;
}

void BaseController::setEdit(std::unique_ptr<te::Edit> edit, bool savePrev) {
    if (edit == nullptr)  // unreadable file, already reported: keep the current edit
        return;

    if (savePrev && edit_ != nullptr)
        EditFileOps::saveEdit(*edit_, true, true, false);

    const auto w = window_.getWidth(), h = window_.getHeight();

    // Release everything referring to the old edit before destroying it
    window_.clearContentComponent();
    MoToolApp::getSelectionManager().deselectAll();
    releaseEditDependents();
    editViewState_.reset();
    playWholeEdit_.reset();

    if (edit_ != nullptr)
        edit_->getTempDirectory(false).deleteRecursively();

    edit_ = std::move(edit);

    rescaleAllMidiClipsToFit(*edit_);
    edit_->playInStopEnabled = true;
    migrateTimecodeDisplaySettings(*edit_);  // a new edit shows bars | beats | frames
    useGlobalMasterVolume();
    edit_->getTransport().ensureContextAllocated();
    edit_->ensureNumberOfAudioTracks(1);
    playWholeEdit_ = std::make_unique<PlayWholeEdit>(*edit_);
    EditFileOps::saveEdit(*edit_, true, true, false);

    editViewState_ = std::make_unique<EditViewState>(*edit_, MoToolApp::getSelectionManager());
    showEdit();
    window_.setSize(w, h);
    window_.repaint();
}

// ================================= MainController =============================================
void ArrangerController::initialize() {
    auto title = String::fromUTF8("Pixel Matter μZX Studio v") + MoToolApp::getApp().getApplicationVersion();
    setMainWindowTitle(title);
    window_.setComponentID("studio");

    #if !JUCE_MAC
    // Set menu bar on the window (macOS uses system menu bar set in AppController)
    window_.setMenuBar(&MoToolApp::getAppController());
    #endif

    BaseController::initialize();
}

ArrangerController::~ArrangerController() {
    #if !JUCE_MAC
    // Clear menu bar from the window
    window_.setMenuBar(nullptr);
    #endif

    // TODO actually we should clean up the render files when closing/destroying an edit, not when closing the app
    // We can subclass the edit and override destructor to do this
    if (edit_ != nullptr) {
        Helpers::removeUnusedRenderFiles(*edit_);
    }

    // Destroy UI components before editViewState_ is implicitly destroyed,
    // since components reference it in their destructors.
    window_.clearContentComponent();
}

void ArrangerController::devicesChanged() {
    // if (!edit_) return;

    // edit_->getTransport().ensureContextAllocated();

    // Do not need this because we can setup this in UI per track
    // if (auto defaultMidiDevice = engine_.getDeviceManager().getDefaultMidiInDevice()) {
    //     // Find the input device instance for the default MIDI device
    //     te::InputDeviceInstance* defaultInstance = nullptr;
    //     for (auto instance : edit_->getAllInputDevices()) {
    //         if (&instance->getInputDevice() == defaultMidiDevice) {
    //             defaultInstance = instance;
    //             break;
    //         }
    //     }

    //     for (auto at : te::getTracksOfType<te::AudioTrack>(*edit_, true)) {
    //         if (at) {
    //             [[maybe_unused]] auto res = defaultInstance->setTarget(at->itemID, false, &edit_->getUndoManager(), 0);
    //         }
    //     }
    // }
    // edit_->restartPlayback();
}

void ArrangerController::handleNew() {
    auto newEdit = createOrLoadEdit(EditFileOps::getTempEditFile());
    setEdit(std::move(newEdit), true);
}

void ArrangerController::handleOpen() {
    auto location = EditFileOps::getRecentEditsDirectory(engine_.getPropertyStorage());
    FileChooser fc("Open file", location, EditFileOps::getAppFileGlob());
    if (fc.browseForFileToOpen()) {
        auto selectedFile = fc.getResult();
        if (selectedFile.existsAsFile()) {
            EditFileOps::setRecentEditsDirectory(engine_.getPropertyStorage(), selectedFile);
        }
        auto newEdit = createOrLoadEdit(selectedFile);
        setEdit(std::move(newEdit), true);
    }
}

void ArrangerController::handleOpenRecent(int fileIndex) {
    auto recentFiles = EditFileOps::getRecentEdits(engine_.getPropertyStorage());
    if (fileIndex >= 0 && fileIndex < recentFiles.size()) {
        File selectedFile(recentFiles[fileIndex]);
        if (selectedFile.existsAsFile()) {
            auto newEdit = createOrLoadEdit(selectedFile);
            setEdit(std::move(newEdit), true);
        } else {
            // File no longer exists, remove from recent files
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                "File Not Found",
                "The file '" + selectedFile.getFileName() + "' could not be found.",
                "OK");
            // The file will be automatically removed next time getRecentEdits() is called
            MoToolApp::getCommandManager().commandStatusChanged(); // Refresh menu
        }
    }
}

void ArrangerController::handleClearRecentFiles() {
    EditFileOps::clearRecentEdits(engine_.getPropertyStorage());
    MoToolApp::getCommandManager().commandStatusChanged(); // Refresh menu to show empty state
}

void ArrangerController::handleSaveAs() {
    if (edit_ == nullptr) return;

    if (EditFileOps::saveEditAsWithDialog(*edit_)) {
        // TODO use listener or callback to update title when edit file changes
        auto name = te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension();
        setMainWindowTitle(name);
    }
}

void ArrangerController::handleRecord() {
    if (edit_ == nullptr) return;

    bool wasRecording = edit_->getTransport().isRecording();
    te::AppFunctions::record();
    if (wasRecording) {
        te::EditFileOperations(*edit_).save(true, true, false);
    }
}

// void handleInsertAudioClip() {
//     Helpers::browseForAudioFile(edit.engine, [this](const File& f) {
//         if (f.existsAsFile()) {
//             auto track = getSelectedOrInsertAudioTrack(edit, selectionManager);
//             te::AudioFile audioFile(edit.engine, f);
//             if (audioFile.isValid()) {
//                 if (auto inserted = track->insertWaveClip(
//                     f.getFileNameWithoutExtension(),
//                     f,
//                     {{{}, te::TimeDuration::fromSeconds(audioFile.getLength())}, {}},
//                     false)
//                 ) {
//                     // DBG("Inserted clip: " << inserted->getName());
//                 }
//             }
//         }
//     });
// }

// void handleInsertMidiClip() {
//     auto seq = Helpers::readMidi(MIDI_CLIP_DATA, 1);
//     auto len = seq.getEndTime();
//     double insertTime = edit.getTransport().getPosition().inSeconds();
//     seq.addTimeToMessages(insertTime);
//     auto time = te::TimeRange(te::TimePosition::fromSeconds(insertTime), te::TimeDuration::fromSeconds(len));
//     auto track = getSelectedOrInsertAudioTrack(edit, selectionManager);

//     if (auto clip = track->insertMIDIClip("MidiClip", time, &selectionManager)) {
//         clip->mergeInMidiSequence(seq, te::MidiList::NoteAutomationType::none);
//         clip->setMidiChannel(te::MidiChannel(1));
//     }
// }

std::unique_ptr<te::Edit> ArrangerController::createOrLoadStartupEdit() {
    return createOrLoadEdit(EditFileOps::getStartupEditFile(engine_.getPropertyStorage()));
}


void ArrangerController::showEdit() {
    window_.setContentOwned(new MainDocumentComponent(*edit_, *editViewState_), true);
    setMainWindowTitle(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension());
}

void ArrangerController::zoomToSelection() {
    auto objects = MoToolApp::getSelectionManager().getSelectedObjects();
    objects = te::getClipSelectionWithCollectionClipContents(objects);
    auto range = te::getTimeRangeForSelectedItems(objects);
    auto viewState = getEditViewState();
    if (viewState != nullptr && !range.isEmpty()) {
        viewState->zoom.setRange(range);
    }
}

}  // namespace MoTool
