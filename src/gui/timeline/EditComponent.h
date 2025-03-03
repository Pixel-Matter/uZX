#pragma once

#include <JuceHeader.h>

#include "../common/Components.h"
#include "Ruler.h"

namespace MoTool {

//==============================================================================
class EditComponent : public Component,
                      private te::ValueTreeAllEventListener,
                      private FlaggedAsyncUpdater,
                      private ChangeListener {
public:
    EditComponent (te::Edit&, te::SelectionManager&);
    ~EditComponent() override;

    EditViewState& getEditViewState()   { return editViewState; }

    void zoomTracksHorizontally(te::TimePosition pos, double factor);
    void zoomToFit();

private:
    void valueTreeChanged() override {}

    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void valueTreeChildOrderChanged(ValueTree&, int, int) override;

    void handleAsyncUpdate() override;
    void resized() override;

    void changeListenerCallback(ChangeBroadcaster*) override { repaint(); }

    void buildTracks();

    void mouseDown(const MouseEvent& e) override;

    te::Edit& edit;

    EditViewState editViewState;
    PlayheadComponent playhead {edit, editViewState};
    RulerComponent ruler {edit, editViewState};

    OwnedArray<TrackComponent> tracks;
    OwnedArray<TrackHeaderComponent> headers;
    OwnedArray<TrackFooterComponent> footers;

    bool updateTracks = false, updateZoom = false;
};

}
