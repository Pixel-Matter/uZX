#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "../common/LookAndFeel.h"
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
    RulerComponent(te::Edit& ed, EditViewState& evs, TimelineGrid& g)
        : te::TempoSequence::Listener {ed.tempoSequence}
        , edit {ed}
        , editViewState {evs}
        , grid {g}
    {
        edit.tempoSequence.addListener(this);
        editViewState.state.addListener(this);
        editViewState.zoom.addListener(this);
        // cached value is ValueTree::Listener
        timecodeFormat.referTo(edit.state, te::IDs::timecodeFormat, nullptr, TimecodeDisplayFormatExt {TimecodeTypeExt::barsBeatsFps50});
    }

    ~RulerComponent() override {
        edit.tempoSequence.removeListener(this);
        editViewState.state.removeListener(this);
        editViewState.zoom.removeListener(this);
    }

    void zoomChanged() override {
        repaint();
    }


    void paint(Graphics& g) override {
        auto ticks = grid.getTicks();
        auto bounds = getLocalBounds();
        g.fillAll(Colors::Theme::background);
        g.setFont(12.0f);

        for (const auto& tick : ticks) {
            if (tick.x > bounds.getWidth())
                continue;
            g.setColour(Colors::Timeline::trackGridTickColors[tick.level]);
            auto y = (float) bounds.getHeight() - (float) bounds.getHeight() / (float) (3 - tick.level);
            g.drawVerticalLine(tick.x, y, (float)bounds.getHeight());
            if (tick.label.isNotEmpty()) {
                g.drawSingleLineText(tick.label, tick.x + 2, 10, Justification::left);
            }
        }
    }

    // void resized() override {
    //     // DBG("RulerComponent::resized");
    //     // repaint();
    // }

    void mouseDown(const MouseEvent& e) override {
        if (e.mods.isPopupMenu()) {
            PopupMenu m;
            m.addItem("Zoom in", [] {
                te::AppFunctions::zoomIn();
            });
            m.addItem("Zoom out", [] {
                te::AppFunctions::zoomOut();
            });
            m.addItem("Zoom to selection", [this] {
                edit.engine.getUIBehaviour().zoomToSelection();
            });
            m.addItem("Zoom fit", [this] {
                edit.engine.getUIBehaviour().zoomToFitHorizontally();
            });
            m.showMenuAsync({});
        } else {
            repositionTransportToX(e.x);
        }
    }

private:
    void repositionTransportToX(int x) {
        auto pos = editViewState.zoom.xToTime(x);
        edit.getTransport().setPosition(pos);
    }

    te::Edit& edit;
    EditViewState& editViewState;
    juce::CachedValue<TimecodeDisplayFormatExt> timecodeFormat;
    TimelineGrid& grid;

    void timerCallback() override {
        repaint();
    }

    // TempoSequenceChange listener implementation
    void selectableObjectChanged(te::Selectable* ) override {
        repaint();
    }

};

} // namespace MoTool
