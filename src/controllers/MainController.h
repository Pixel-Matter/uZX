#pragma once

#include <JuceHeader.h>

#include "EditState.h"

#include "../gui/main/MainWindow.h"


namespace MoTool {

class BaseController : private ChangeListener,
                       public MenuBarModel,
                       public ApplicationCommandTarget {
public:
    BaseController();

    ~BaseController() override;

    // Two phase initialization becase of virtual functions called in initialization
    virtual void initialize();

    te::Edit* getEdit();
    te::Engine& getEngine();
    te::SelectionManager& getSelectionManager();
    ApplicationCommandManager& getCommandManager();
    // FIXME EditViewState is too specific for base controller
    virtual EditViewState* getEditViewState();

    void setMainWindowTitle(const String& title);

protected:
    virtual std::unique_ptr<te::Edit> createOrLoadStartupEdit() = 0;
    virtual std::unique_ptr<te::Edit> createOrLoadEdit(File editFile);
    virtual void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) = 0;
    virtual void devicesChanged();
    void handlePluginManager();

    te::Engine engine_;
    ApplicationCommandManager commandManager_;
    te::SelectionManager selectionManager_ {engine_};
    MainWindow mainWindow_;
    std::unique_ptr<te::Edit> edit_;
    // FIXME EditViewState is too specific for base controller
    std::unique_ptr<EditViewState> editViewState_;

private:
    // Called when the selection changes
    void changeListenerCallback (ChangeBroadcaster*) override;
};


class MainController : public BaseController {
public:
    using BaseController::BaseController;

    ~MainController() override;

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

private:
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) override;
    void handleNew();
    void handleOpen();
    void handleSaveAs();
    void handleRecord();
    void createTracksAndAssignInputs();
    // void devicesChanged() override;
};


}  // namespace MoTool