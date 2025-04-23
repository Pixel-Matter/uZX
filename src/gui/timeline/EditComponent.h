#pragma once

#include <JuceHeader.h>

#include "../common/Components.h"
#include "TrackComponents.h"
#include "DetailsPanelComponent.h"
#include "Ruler.h"

namespace MoTool {

//==============================================================================
class EditComponent final : public Component,
                      //   private ApplicationCommandTarget
                      private FlaggedAsyncUpdater,  // for marking and updating asynchronously
                    //   private ComponentListener,
                      //   private ChangeListener,
                      //   private ZoomViewState::Listener
                      private ValueTree::Listener
                    {
public:
    EditComponent(te::Edit&, EditViewState&);
    ~EditComponent() override;

private:
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;

    // void zoomChanged() override;
    // void componentMovedOrResized(Component&, bool /*wasMoved*/, bool /*wasResized*/) override;
    void handleAsyncUpdate() override;
    void resized() override;
    // void mouseDown(const MouseEvent& e) override;

    // implementation for ApplicationCommandTarget
    // ApplicationCommandTarget* getNextCommandTarget() override;
    // void getAllCommands(Array<CommandID>&) override;
    // void getCommandInfo(CommandID, ApplicationCommandInfo&) override;
    // bool perform(const InvocationInfo&) override;

    te::Edit& edit;
    EditViewState& editViewState;

    PlayheadComponent playhead {edit, editViewState};
    RulerComponent ruler {edit, editViewState};

    Viewport trackViewport;  // TODO use this to scroll tracks
    TracksContainerComponent tracksContainer {edit, editViewState, ruler};
    DetailsPanelComponent detailsPanel {editViewState};

    bool updateSizes = false;
};

}
