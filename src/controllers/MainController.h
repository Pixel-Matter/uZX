#pragma once

#include <JuceHeader.h>

#include "EditState.h"
#include "PropertyStorage.h"

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
    void showAboutDialog();

    te::Engine engine_;
    ApplicationCommandManager commandManager_;
    te::SelectionManager selectionManager_ {engine_};

    std::unique_ptr<TuningController> tuningController_;

    // Called when the selection changes
    void changeListenerCallback(ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppController)
};


class BaseController : private ChangeListener {
public:
    explicit BaseController(te::Engine& e);
    ~BaseController() override;

    // Two phase initialization becase of virtual functions called in initialization
    virtual void initialize();

    te::Edit* getEdit();
    te::Engine& getEngine();

    MainWindow* getMainWindow() { return &window_; }
    void setMainWindowTitle(const String& title);

    void bringWindowToFront();

    virtual void zoomHorizontal(float) {}
    virtual void zoomVertical(float) {}
    virtual void zoomToSelection() {}
    virtual void zoomToFitHorizontally() {}
    virtual void zoomToFitVertically() {}

protected:
    virtual std::unique_ptr<te::Edit> createOrLoadStartupEdit() = 0;
    virtual std::unique_ptr<te::Edit> createOrLoadEdit(File editFile);
    virtual void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) = 0;
    virtual void devicesChanged() {}

    te::Engine& engine_;
    MainWindow window_ {engine_};
    std::unique_ptr<te::Edit> edit_;

private:
    // Called when the selection changes
    void changeListenerCallback(ChangeBroadcaster*) override;
};


class ArrangerController : public BaseController {
public:
    using BaseController::BaseController;

    ~ArrangerController() override;

    void initialize() override;
    EditViewState* getEditViewState();

    void zoomHorizontal(float increment) override;
    void zoomToSelection() override;
    void zoomToFitHorizontally() override;

    void handleNew();
    void handleOpen();
    void handleOpenRecent(int fileIndex);
    void handleClearRecentFiles();
    void handleSaveAs();
    void handleRecord();

private:
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) override;
    void createTracksAndAssignInputs();

    void devicesChanged() override;

    std::unique_ptr<EditViewState> editViewState_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangerController)
};


}  // namespace MoTool
