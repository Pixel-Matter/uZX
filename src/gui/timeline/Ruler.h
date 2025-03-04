#pragma once

#include <JuceHeader.h>

#include "../common/LookAndFeel.h"
#include "../common/Components.h"
#include "juce_data_structures/juce_data_structures.h"

namespace MoTool {

class RulerComponent : public Component,
                       private Timer,
                       private te::TempoSequence::Listener,
                       private ValueTree::Listener {
public:
    RulerComponent(te::Edit& ed, EditViewState& evs)
        : te::TempoSequence::Listener {ed.tempoSequence}
        , edit {ed}
        , editViewState {evs}
    {
        edit.tempoSequence.addListener(this);
        editViewState.state.addListener(this);
        // startTimerHz(30);
    }

    ~RulerComponent() override {
        edit.tempoSequence.removeListener(this);
        editViewState.state.removeListener(this);
        stopTimer();
    }

    void paint(Graphics& g) override {
        auto bounds = getLocalBounds();
        auto &lf = getLookAndFeel();

        g.setColour(lf.findColour(ResizableWindow::backgroundColourId));
        g.fillRect(bounds);

        te::TimePosition startTime = editViewState.viewX1;
        te::TimeDuration viewLength = editViewState.viewLength();

        // Draw major beat lines (bars)
        g.setColour(Colors::Theme::border);
        int beatNumber = 0;
        auto currentTime = startTime;
        const int width = bounds.getWidth();
        const float height = (float)bounds.getHeight();
        g.setFont(10.0f);

        while (currentTime < startTime + viewLength) {
            double beatsPerSec = edit.tempoSequence.getBeatsPerSecondAt(currentTime);
            auto timePerBeat = te::TimeDuration::fromSeconds(1.0f / beatsPerSec);
            float pixelsPerBeat = editViewState.pixelsPerBeat(timePerBeat, width);
            float x = (float)editViewState.timeToX(currentTime, width);

            if (beatNumber % 4 == 0) { // TODO get TimeSig from editViewState
                // Bar line (assuming 4/4 time)
                g.drawLine(x, 0, x, height, 1.0f);

                // Draw bar number
                String barText = String(beatNumber / 4 + 1);
                g.drawText(barText, Rectangle<float>(x + 2, -4, 20, 20), Justification::left);
            } else {
                // Beat line
                g.drawLine(x, height * 0.5f, x, height, 1.0f);
            }

            // TODO draw frames
            float pixelsPerSubdiv = pixelsPerBeat;
            int numSubdivs = 1;
            while (numSubdivs < 16 && pixelsPerSubdiv > 8.0f) {
                numSubdivs *= 2;
                pixelsPerSubdiv /= 2;
            }
            for (int sub = 1; sub < numSubdivs; ++sub) {
                float subX = x + (pixelsPerBeat * (float)sub / (float)numSubdivs);
                g.drawLine(subX, height * 0.75f, subX, height, 0.5f);
            }
            currentTime = currentTime + timePerBeat;
            beatNumber++;
        }
    }

    void resized() override {
        repaint();
    }

    void mouseDown(const MouseEvent& e) override {
        if (e.mods.isPopupMenu()) {
            PopupMenu m;
            m.addItem("Zoom In", [this] {
                editViewState.zoomHorizontally(edit.getTransport().getPosition(), 1.0 / 1.25);
            });
            m.addItem("Zoom Out", [this] {
                editViewState.zoomHorizontally(edit.getTransport().getPosition(), 1.25);
            });
            m.addItem("Zoom Fit", [this] {
                editViewState.viewX1 = 0s;
                editViewState.viewX2 = 60s;
                // TODO get global start and end
                // editViewState.zoomHorizontally(factor);
            });
            m.showMenuAsync({});
        } else {
            repositionTransportToX(e.x);
        }
    }

    void repositionTransportToX(int x) {
        auto pos = editViewState.xToTime(x, getWidth());
        edit.getTransport().setPosition(pos);
    }

private:
    te::Edit& edit;
    EditViewState& editViewState;

    void timerCallback() override {
        repaint();
    }

    // TempoSequenceChange listener implementation
    void selectableObjectChanged(te::Selectable* ) override {
        repaint();
    }

    void valueTreePropertyChanged(ValueTree&, const Identifier& prop) override {
        if (prop == IDs::viewX1 || prop == IDs::viewX2) {
            repaint();
        }
    }
};

} // namespace MoTool
