#pragma once

#include <JuceHeader.h>

#include "App.h"

#include "../gui/common/ProgressDialog.h"
#include "../gui/common/Utilities.h"
#include "../util/Helpers.h"

#include <common/PluginWindow.h>  // from JUCE
#include <memory>

namespace MoTool {


class ExtUIBehaviour : public te::UIBehaviour {
public:
    ExtUIBehaviour() = default;

    // Only single edit can be opened at a time
    te::Edit* getCurrentlyFocusedEdit()                                   override {
        return MoToolApp::getArrangerController().getEdit();
    }
    te::Edit* getLastFocusedEdit()                                        override {
        return getCurrentlyFocusedEdit();
    }

    juce::Array<te::Edit*> getAllOpenEdits()                              override {
        return {getLastFocusedEdit()};
    }

    bool isEditVisibleOnScreen(const te::Edit&)                           override { return true; }
    bool closeAllEditsBelongingToProject(te::Project&)                    override { return false; }
    void editNamesMayHaveChanged()                                        override {}

    te::SelectionManager* getCurrentlyFocusedSelectionManager()           override {
        return &MoToolApp::getAppController().getSelectionManager();
    }

    te::SelectionManager* getSelectionManagerForRack(const te::RackType&) override { return {}; }
    // bool paste(const te::Clipboard&) override;

    te::Project::Ptr getCurrentlyFocusedProject()                         override { return {}; }
    void selectProjectInFocusedWindow(te::Project::Ptr)                   override {}
    void updateAllProjectItemLists()                                      override {}

    juce::ApplicationCommandManager* getApplicationCommandManager() override {
        return &MoToolApp::getCommandManager();
    }

    void getAllCommands(juce::Array<juce::CommandID>& /*commands*/) override {
        // if (auto* win = MoToolApp::getApp().getMainWindow())
        //     return win->getAllCommands(commands);
    }
    void getCommandInfo(juce::CommandID /*cmd*/, juce::ApplicationCommandInfo& /*result*/) override {
        // if (auto* win = MoToolApp::getApp().getMainWindow())
        //     win->getCommandInfo(cmd, result);
    }
    bool perform(const juce::ApplicationCommandTarget::InvocationInfo& /*info*/) override {
        // if (auto* win = MoToolApp::getApp().getMainWindow())
        //     return win->perform(info);
        return false;
    }

    //==============================================================================
    /** Should show the new plugin window and creates the Plugin the user selects. */
    // TODO implement
    te::Plugin::Ptr showMenuAndCreatePlugin(te::Plugin::Type, te::Edit&)  override { return {}; }

    /** Must create a suitable Component plugin window for the given PluginWindowState.
        The type of state should be checked and used accordingly e.g. Plugin::WindowState
        or RackType::WindowsState
    */
    std::unique_ptr<Component> createPluginWindow(te::PluginWindowState& pws) override {
        if (auto ws = dynamic_cast<te::Plugin::WindowState*>(&pws))
            return PluginWindow::create(ws->plugin);

        return {};
    }

    void recreatePluginWindowContentAsync(te::Plugin& p) override {
        if (auto* w = dynamic_cast<PluginWindow*>(p.windowState->pluginWindow.get()))
            return w->recreateEditorAsync();

        UIBehaviour::recreatePluginWindowContentAsync(p);
    }

    /** Called when a new track is created from some kind of user action i.e. not from an Edit load. */
    void newTrackCreated(te::Track&) override {}

    /** Should show the current quantisation level for a short period of time. */
    void showQuantisationLevel()     override {}

    /** Should run this task in the current window, with a progress bar, blocking
        until the task is done.
    */
    void runTaskWithProgressBar(te::ThreadPoolJobWithProgress& job) override {
        auto& engine = MoToolApp::getArrangerController().getEngine();
        auto& jobManager = engine.getBackgroundJobs();
        jobManager.addJob(&job, false);

        ProgressDialog dialog {job.getJobName(), job, jobManager};
        dialog.setVisible(true);
        dialog.runModalLoop();

        job.prepareForJobDeletion();
        jobManager.removeJob(&job, false, 1000);
    }

    bool getBigInputMetersMode()                                override { return false; }

    void setBigInputMetersMode(bool)                            override {}
    bool shouldGenerateLiveWaveformsWhenRecording()             override { return true;  }

    void showSafeRecordDialog(te::TransportControl&)            override {}
    void hideSafeRecordDialog(te::TransportControl&)            override {}

    void showProjectScreen()                                    override {}

    void showSettingsScreen() override {
        DialogWindow::LaunchOptions o;
        o.dialogTitle = TRANS("Audio Settings");
        o.dialogBackgroundColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
        o.content.setOwned(
            new AudioDeviceSelectorComponent(MoToolApp::getArrangerController().getEngine().getDeviceManager().deviceManager,
                                             0, 512, 1, 512,
                                             true, true, true, false));
        o.useNativeTitleBar = true;
        o.escapeKeyTriggersCloseButton = true;
        o.resizable = true;
        o.content->setSize(500, 600);
        o.launchAsync();
    }

    void showEditScreen()                                       override {}

    void showHideVideo()                                        override {}
    void showHideInputs()                                       override {}
    void showHideOutputs()                                      override {}
    void showHideMixer(bool /*fullscreen*/)                     override {}
    void showHideMidiEditor(bool /*fullscreen*/)                override {}
    void showHideTrackEditor(bool /*zoom*/)                     override {}
    void showHideBrowser()                                      override {}
    void showHideActions()                                      override {}
    void showHideAllPanes()                                     override {}
    // TODO implement
    void toggleScroll()                                         override {}
    bool isScrolling()                                          override { return false; }

    //==============================================================================
    /** Get called by ControlSurface::userPressedUserAction(int action)
        via AppFunction::performUserAction(int action)
    */
    void performUserAction (int /*action*/)                     override {}

    void scrollTracksUp()                                       override {}
    void scrollTracksDown()                                     override {}
    void scrollTracksLeft()                                     override {}
    void scrollTracksRight()                                    override {}

    // void nudgeSelectedClips (TimecodeSnapType, const juce::String& commandDesc,
    //                                  SelectionManager&, const juce::Array<Clip*>&, bool automationLocked) override;
    // void nudgeSelected (TimecodeSnapType, const juce::String& commandDesc, bool automationLocked) override;
    // void nudgeSelected (const juce::String& commandDesc) override;

    void stopPreviewPlayback()                                  override {}
    void resetOverloads()                                       override {}
    void resetPeaks()                                           override {}

    void zoomHorizontal(float amount)                          override {
        MoToolApp::getArrangerController().zoomHorizontal(amount);
    }

    void zoomVertical(float amount)                            override {
        MoToolApp::getArrangerController().zoomVertical(amount);
    }

    void zoomToSelection()                                      override {
        MoToolApp::getArrangerController().zoomToSelection();
    }

    void zoomToFitHorizontally()                                override {
        MoToolApp::getArrangerController().zoomToFitHorizontally();
    }

    void zoomToFitVertically()                                  override {
        MoToolApp::getArrangerController().zoomToFitVertically();
    }

    //==============================================================================
    /** Should return the position which used be used for edit operations such as splitting.
        By default this returns the transport position.
    */
    // te::TimePosition getEditingPosition(te::Edit&) override;

    /** Should return the range which used be used for edit operations such as coping or deleting.
        By default this returns the loop range.
    */
    // te::TimeRange getEditingRange(te::Edit&) override;

    /** Can return a range of tracks which used be used for edit operations such as coping or deleting.
    */
    juce::Array<te::Track*> getEditingTracks(te::Edit&)                         override { return {}; }

    //==============================================================================
    /** If your UI has the concept of edit groups, you should return an expanded list of
        selected items that includes all clips that should be edited with the selected
        clip */
    te::SelectableList getAssociatedClipsToEdit(const te::SelectableList& items) override { return items; }

};


}  // namespace MoTool