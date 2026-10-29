#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "TimelineGrid.h"


namespace MoTool {

class RulerComponent : public Component,
                       private ZoomViewState::Listener,
                       private TimelineGrid::Listener
{
public:
    RulerComponent(te::Edit& ed, EditViewState& evs, TimelineGrid& g);
    ~RulerComponent() override;

    void zoomChanged() override;
    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override;
    void mouseMagnify(const MouseEvent& e, float scale) override;

private:
    void repositionTransportToX(int x);
    void gridChanged() override;

    te::Edit& edit;
    EditViewState& editViewState;
    TimelineGrid& grid;

    bool isDragging = false;
    static constexpr int dragThreshold = 4;
};

} // namespace MoTool
