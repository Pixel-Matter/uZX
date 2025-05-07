#pragma once

#include <JuceHeader.h>

#include "EditState.h"

#include "../gui/main/MainWindow.h"


namespace MoTool {

class MainController : public MenuBarModel,
                       public ApplicationCommandTarget,
                       private ChangeListener {
public:
    MainController();

    ~MainController() override;

    void setMainWindowTitle(const String& title);

    te::Edit* getEdit();
    te::Engine& getEngine();
    EditViewState* getEditViewState();
    te::SelectionManager& getSelectionManager();
    ApplicationCommandManager& getCommandManager();

    // ==============================================================================
    // MenuBarModel
    //==============================================================================
    StringArray getMenuBarNames() override;
    PopupMenu getMenuForIndex(int /* menuIndex */, const String& menuName) override;
    void menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) override;

    // ==============================================================================
    // ApplicationCommandTarget
    //==============================================================================
    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands(Array<CommandID>& commands) override;
    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override;
    bool perform(const InvocationInfo& info) override;

private:
    te::Engine engine_;
    ApplicationCommandManager commandManager_;
    te::SelectionManager selectionManager_ {engine_};

    MainWindow mainWindow_;

    // TODO refactor to EditController?
    std::unique_ptr<te::Edit> edit_;
    std::unique_ptr<EditViewState> editViewState_;

    void handleNew();
    void handleOpen();
    void handleSaveAs();
    void handleRecord();
    void hanldePluginManager();

    std::unique_ptr<te::Edit> createOrLoadEdit(File editFile);

    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false);
    void createTracksAndAssignInputs();

    // Called when the selection changes
    void changeListenerCallback (ChangeBroadcaster*) override;
};


}  // namespace MoTool