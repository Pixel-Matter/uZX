#pragma once

#include <JuceHeader.h>

#include "TrackComponents.h"
#include "DetailsPanelComponent.h"
#include "Ruler.h"
#include "PlayheadComponent.h"
#include "TimelineGrid.h"

namespace MoTool {

struct EditComponentOptions {
    bool showDetailsPanel = true;
    bool autoFitTrackHeights = false;
    bool transparentClips = false;
    bool disableClipSelection = false;
};

//==============================================================================
/**
 * Main component for the timeline edit view, containing tracks, ruler, and playhead
 */
class EditComponent final : public Component,
                      private FlaggedAsyncUpdater,  // for marking and updating asynchronously
                      private ValueTree::Listener,
                      private ZoomViewState::Listener,
                      private ScrollBar::Listener
                    {
public:
    EditComponent(te::Edit&, EditViewState&, EditComponentOptions opts = {});
    ~EditComponent() override;

private:
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void zoomChanged() override;
    void updateScrollBar();
    void scrollBarMoved(ScrollBar*, double newRangeStart) override;

    void handleAsyncUpdate() override;
    void resized() override;
    // middle-button view navigation anywhere in the tracks, which the clips and track bodies leave alone
    void mouseDown(const MouseEvent&) override;
    void mouseDrag(const MouseEvent&) override;
    void mouseUp(const MouseEvent&) override;
    void paint(Graphics& g) override;

    EditComponentOptions options;
    te::Edit& edit;
    EditViewState& editViewState;
    TimelineGrid grid {editViewState};

    class ZoomableViewport : public Viewport {
    public:
        ZoomableViewport(EditViewState& evs) : editViewState(evs) {}
        void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
        void mouseMagnify(const MouseEvent& e, float scale) override;
    private:
        EditViewState& editViewState;
    };

    PlayheadComponent playhead {edit, editViewState, grid};
    RulerComponent ruler {edit, editViewState, grid};
    TracksContainerComponent tracksContainer {edit, editViewState, grid};
    ZoomableViewport trackViewport {editViewState};
    DetailsPanelComponent detailsPanel {editViewState, grid};

    /** Drags hold the view like ruler drags; wheel and pinch act as on the ruler. */
    class TimeScrollBar : public ScrollBar {
    public:
        TimeScrollBar(ZoomViewState& z, RulerComponent& r) : ScrollBar(false), zoom(z), ruler(r) {}
        void mouseDown(const MouseEvent& e) override;
        void mouseUp(const MouseEvent& e) override;
        void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
        void mouseMagnify(const MouseEvent& e, float scale) override;
    private:
        ZoomViewState& zoom;
        RulerComponent& ruler;
    };

    TimeScrollBar timeScrollBar {editViewState.zoom, ruler};

    bool updateSizes = false, updateScrollRange = false;
    bool middleDragging = false;
};

}
