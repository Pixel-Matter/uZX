#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"

namespace MoTool {

class TimelineGrid;

//==============================================================================
class PlayheadComponent : public Component,
                          private ZoomViewState::Listener {
public:
    PlayheadComponent(te::Edit&, EditViewState&, TimelineGrid&);
    ~PlayheadComponent() override;

    void paint(Graphics& g) override;
    bool hitTest(int x, int y) override;
    void mouseEnter(const MouseEvent&) override;
    void mouseDrag(const MouseEvent&) override;
    void mouseDown(const MouseEvent&) override;
    void mouseUp(const MouseEvent&) override;
    void mouseWheelMove(const MouseEvent&, const MouseWheelDetails&) override;
    void mouseMagnify(const MouseEvent&, float scale) override;

private:
    void viewRangeChanged() override;
    void playheadMoved() override;

    void checkRepaint();
    Component* getSiblingUnder(const MouseEvent&) const;

    te::Edit& edit;
    EditViewState& editViewState;
    TimelineGrid& grid;

    int xPosition = 0;
    // bool firstTimer = true;
};

}  // namespace MoTool
