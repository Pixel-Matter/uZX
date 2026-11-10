#pragma once

#include <JuceHeader.h>

#include "EditState.h"
#include "PropertyStorage.h"
#include "PlayWholeEdit.h"

#include "../gui/main/MainWindow.h"

namespace MoTool {

class TuningController;

class AppController : public MenuBarModel,
                      public ApplicationCommandTarget,
                      private ChangeListener
{
public:
    AppController();
    ~AppController() override;

    // Two phase initialization becase of virtual functions called in initialization
    virtual void initialize();

    te::Engine& getEngine();
    te::SelectionManager& getSelectionManager();
    ApplicationCommandManager& getCommandManager();

    //==============================================================================
    /**
     * MenuBarModel implementation
     */
    StringArray getMenuBarNames() override;
    PopupMenu getMenuForIndex(int /* menuIndex */, const String& menuName) override;
    void menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) override;

    //==============================================================================
    /**
     * ApplicationCommandTarget implementation
     */
    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands(Array<CommandID>& commands) override;
    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override;
    bool perform(const InvocationInfo& info) override;

    void openTuningWindow();
    void closeTuningWindow();

private:
    void handlePluginManager();
    void ensureMinimumSampleRate();
    /** Drops a saved input device that differs from the output device (unplayable on macOS). */
    void sanitizeAudioInputDevice();
    void showAboutDialog();

    /** Returns an error description if the audio output device is not usable, or an empty
        string when audio is working. When requireRunning is true, a device whose callback is
        not currently running is also treated as an error (use before starting playback). */
    String getAudioDeviceError(bool requireRunning = false) const;
    /** Shows an alert about the audio problem and offers to open the Audio/MIDI settings screen. */
    void reportAudioDeviceProblem(const String& error);
    /** Verifies audio is usable before starting playback; returns true if it is safe to play. */
    bool ensureAudioReadyForPlayback();

    te::Engine engine_;
    ApplicationCommandManager commandManager_;
    te::SelectionManager selectionManager_ {engine_};

    std::unique_ptr<TuningController> tuningController_;

    // Called when the selection changes
    void changeListenerCallback(ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppController)
};


class BaseController : private ChangeListener,
                       private Value::Listener
{
public:
    explicit BaseController(te::Engine& e);
    ~BaseController() override;

    // Two phase initialization becase of virtual functions called in initialization
    virtual void initialize();

    te::Edit* getEdit();
    EditViewState* getEditViewState() { return editViewState_.get(); }
    te::Engine& getEngine();

    MainWindow* getMainWindow() { return &window_; }
    void setMainWindowTitle(const String& title);

    void bringWindowToFront();

    void zoomHorizontal(float increment);
    virtual void zoomToSelection() {}
    void zoomToFitHorizontally();

protected:
    virtual std::unique_ptr<te::Edit> createOrLoadStartupEdit() = 0;
    /** Nullptr, so setEdit() keeps the current edit, for a file that is unreadable (reported) or already open. */
    virtual std::unique_ptr<te::Edit> createOrLoadEdit(File editFile);
    /** Replaces edit_, keeping the current edit when edit is null (unreadable, already reported).
        Saves the old edit if asked and releases everything referring to it (window content,
        selection, releaseEditDependents(), view and playback state) before destroying it and its
        temp folder; then prepares and saves the new edit, gives it a view state and calls showEdit().
        The window keeps its size. */
    virtual void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false);
    /** Drops the app's own references to edit_ before it is replaced; editViewState_ is still alive. */
    virtual void releaseEditDependents() {}
    /** Builds the app's own state for the new edit_, the window content and the title. */
    virtual void showEdit() = 0;
    virtual void devicesChanged() {}

    te::Engine& engine_;
    MainWindow window_ {engine_};
    std::unique_ptr<te::Edit> edit_;
    std::unique_ptr<PlayWholeEdit> playWholeEdit_;
    std::unique_ptr<EditViewState> editViewState_;  // after edit_, so destroyed before it

private:
    // Called when the selection changes
    void changeListenerCallback(ChangeBroadcaster*) override;
    void valueChanged(Value&) override;

    /** Master volume is an app setting, not part of the file: applies the saved level to edit_
        and saves the knob from now on. */
    void useGlobalMasterVolume();

    Value masterVolume_;  // edit_'s master volume slider position
};


class ArrangerController : public BaseController {
public:
    using BaseController::BaseController;

    ~ArrangerController() override;

    void initialize() override;

    void zoomToSelection() override;

    void handleNew();
    void handleOpen();
    void handleOpenRecent(int fileIndex);
    void handleClearRecentFiles();
    void handleSaveAs();
    void handleRecord();

private:
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void showEdit() override;

    void devicesChanged() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangerController)
};

void registerPlugins(te::Engine& engine);

}  // namespace MoTool
