#pragma once

#include <JuceHeader.h>

#include "../common/LookAndFeel.h"
#include "../common/Components.h"
#include "../common/Utilities.h"
#include "juce_data_structures/juce_data_structures.h"
#include "tracktion_core/utilities/tracktion_Tempo.h"
#include "tracktion_core/utilities/tracktion_Time.h"

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
        using namespace te::tempo;
        auto bounds = getLocalBounds();
        auto &lf = getLookAndFeel();
        auto& ts = edit.tempoSequence;

        constexpr auto minPixelsPerBeat = 16.0f;
        const int width = bounds.getWidth();
        const float height = (float)bounds.getHeight();

        g.setColour(lf.findColour(ResizableWindow::backgroundColourId));
        g.fillRect(bounds);
        g.setFont(12.0f);

        auto startBar = ts.toBarsAndBeats(editViewState.viewX1.get());
        // DBG("Ruler start beats " << startBarBeats.bars << "|" << startBarBeats.getWholeBeats() << "|" << startBarBeats.getFractionalBeats());
        startBar.beats = te::BeatDuration::fromBeats(0);
        // DBG("Ruler start beats rounded " << startBarBeats.bars << "|" << startBarBeats.getWholeBeats() << "|" << startBarBeats.getFractionalBeats());
        auto startTime = ts.toTime(startBar);
        DBG("Ruler start time " << startTime.inSeconds());

        auto currentTime = startTime;
        auto beatStep = te::BeatDuration::fromBeats(0.25);

        while (currentTime <= editViewState.viewX2.get()) {
            auto barBeats = ts.toBarsAndBeats(currentTime);

            auto nextDiv = BarsAndBeats { barBeats.bars, barBeats.beats + beatStep };
            auto nextTime = ts.toTime(nextDiv);
            auto pixelsPerDiv = editViewState.durationToPixels(nextTime - currentTime, width);
            if (pixelsPerDiv < minPixelsPerBeat) {
                beatStep = beatStep * 2.0;
                DBG("PixelsPerDiv == " << pixelsPerDiv << ", adjusting beat step " << beatStep);
                continue;
            }

            DBG("Beats " << barBeats.bars << "|" << barBeats.getWholeBeats() << "|" << barBeats.getFractionalBeats());
            DBG("Time " << currentTime.inSeconds());
            DBG("Next div " << nextDiv.bars << "|" << nextDiv.getWholeBeats() << "|" << nextDiv.getFractionalBeats());
            DBG("Next time " << nextTime.inSeconds());

            auto x = static_cast<float>(editViewState.timeToX(currentTime, width));
            currentTime = nextTime;

            if (x < 0) {
                continue;
            }
            if (barBeats.beats < te::BeatDuration::fromBeats(0.001)) {
                // Bar line
                g.setColour(Colors::Theme::borderLight);
                g.drawLine(x, 0, x, height, 1.0f);
                // Draw bar number
                String barText = String(barBeats.bars + 1);
                g.drawText(barText, Rectangle<float>(x + 2, -4, 20, 20), Justification::left);
            } else {
                // Beat line
                g.setColour(Colors::Theme::border);
                g.drawLine(x, height * 0.5f, x, height, 1.0f);
            }

            // g.setColour(Colors::Theme::border);
            // // TODO draw frames
            // float pixelsPerSubdiv = pixelsPerBeat;
            // int numSubdivs = 1;
            // while (numSubdivs < 16 && pixelsPerSubdiv > minPixelsPerDiv) {
            //     numSubdivs *= 2;
            //     pixelsPerSubdiv /= 2;
            // }
            // for (int sub = 1; sub < numSubdivs; ++sub) {
            //     float subX = x + (pixelsPerBeat * (float)sub / (float)numSubdivs);
            //     g.drawLine(subX, height * 0.75f, subX, height, 0.5f);
            // }
            // currentTime = currentTime + timePerBeat;
            // beatNumber++;
            // break;
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
                auto range = Helpers::getEffectiveClipsTimeRange(edit);
                if (!range.isEmpty()) {
                    editViewState.viewX1 = range.getStart();
                    editViewState.viewX2 = range.getEnd();
                }
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
