#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "juce_events/juce_events.h"

namespace MoTool {

//==============================================================================
class PlayheadComponent : public Component,
                          private ZoomViewState::Listener {
public:
    PlayheadComponent(te::Edit&, EditViewState&);
    ~PlayheadComponent() override;

    void paint(Graphics& g) override;
    bool hitTest(int x, int y) override;
    void mouseEnter(const MouseEvent&) override;
    void mouseDrag(const MouseEvent&) override;
    void mouseDown(const MouseEvent&) override;
    void mouseUp(const MouseEvent&) override;

private:
    void zoomChanged() override;
    void zoomOrPosChanged() override;

    void checkRepaint();

    te::Edit& edit;
    EditViewState& editViewState;

    int xPosition = 0;
    // bool firstTimer = true;
};

}  // namespace MoTool
