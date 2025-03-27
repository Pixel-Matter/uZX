#pragma once

#include <JuceHeader.h>

#include "../common/Components.h"
#include "TrackComponents.h"
#include "Ruler.h"

namespace MoTool {

//==============================================================================
class EditComponent final : public Component,
                      private te::ValueTreeAllEventListener,
                      private ZoomViewState::Listener,
                      private FlaggedAsyncUpdater,  // for marking and updating asynchronously
                      private ChangeListener
                    //   private ApplicationCommandTarget
                      {
public:
    EditComponent(te::Edit&, EditViewState&);
    ~EditComponent() override;

    EditViewState& getEditViewState()   { return editViewState; }

    // void zoomTracksHorizontally(double factor);
    // void zoomToFit();

private:
    void buildTracks();

    void valueTreeChanged() override {}
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void valueTreeChildOrderChanged(ValueTree&, int, int) override;

    void zoomChanged() override {
        // DBG("EditComponent::zoomChanged");
        markAndUpdate(updateZoom);
    }

    void handleAsyncUpdate() override;
    void resized() override;

    void changeListenerCallback(ChangeBroadcaster*) override { repaint(); }

    void mouseDown(const MouseEvent& e) override;

    // implementation for ApplicationCommandTarget
    // ApplicationCommandTarget* getNextCommandTarget() override;
    // void getAllCommands(Array<CommandID>&) override;
    // void getCommandInfo(CommandID, ApplicationCommandInfo&) override;
    // bool perform(const InvocationInfo&) override;

    te::Edit& edit;
    EditViewState& editViewState;

    PlayheadComponent playhead {edit, editViewState};
    RulerComponent ruler {edit, editViewState};

    OwnedArray<TrackComponent> tracks;
    OwnedArray<TrackHeaderComponent> headers;
    OwnedArray<TrackFooterComponent> footers;

    bool updateTracks = false, updateZoom = false;
};

}
