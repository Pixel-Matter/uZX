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

private:
    void repositionTransportToX(int x);
    void timerCallback() override;
    void selectableObjectChanged(te::Selectable*) override;

    te::Edit& edit;
    EditViewState& editViewState;
    juce::CachedValue<TimecodeDisplayFormatExt> timecodeFormat;
    TimelineGrid& grid;
};

} // namespace MoTool
