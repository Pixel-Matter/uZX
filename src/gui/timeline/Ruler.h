#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "../../models/Timecode.h"
#include "TimelineGrid.h"


namespace MoTool {

class RulerComponent : public Component,
                       private Timer,
                       private te::TempoSequence::Listener,
                       private ValueTree::Listener,
                       private ZoomViewState::Listener
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

private:
    void repositionTransportToX(int x);
    void timerCallback() override;
    void selectableObjectChanged(te::Selectable*) override;

    te::Edit& edit;
    EditViewState& editViewState;
    juce::CachedValue<TimecodeDisplayFormatExt> timecodeFormat;
    TimelineGrid& grid;

    bool isDragging = false;
    te::TimePosition dragStartViewStart;
    te::TimeDuration dragStartTimePerPixel;
    static constexpr int dragThreshold = 4;
};

} // namespace MoTool
