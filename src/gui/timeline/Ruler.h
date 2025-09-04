#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "../common/LookAndFeel.h"
#include "../../models/Timecode.h"


namespace MoTool {

class RulerComponent : public Component,
                       private Timer,
                       private te::TempoSequence::Listener,
                       private ValueTree::Listener,
                       private ZoomViewState::Listener
{
public:
    RulerComponent(te::Edit& ed, EditViewState& evs)
        : te::TempoSequence::Listener {ed.tempoSequence}
        , edit {ed}
        , editViewState {evs}
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
        // DBG("RulerComponent::paint");
        using namespace te::tempo;

        double fps = timecodeFormat->getFPS();
        constexpr float minPxPerDev = 6.0f;

        auto& zoomState = editViewState.zoom;
        auto bounds = getLocalBounds();
        auto &lf = getLookAndFeel();
        auto& ts = edit.tempoSequence;

        const int width = bounds.getWidth();
        const float height = (float)bounds.getHeight();
        // auto beatStep = te::BeatDuration::fromBeats(1.0 / 26);  // Tiratok
        auto beatStep = te::BeatDuration::fromBeats(1.0 / 2);  // max 64 subdivs in beat

        g.setColour(lf.findColour(ResizableWindow::backgroundColourId));
        g.fillRect(bounds);
        g.setFont(12.0f);

        // DBG("RulerComponent::paint: " << zoomState.getRangeStart().inSeconds() << " - " << zoomState.getRangeEnd().inSeconds());

        auto startBar = ts.toBarsAndBeats(zoomState.xToTime(0));
        startBar = te::tempo::BarsAndBeats(startBar.bars, te::BeatDuration::fromBeats(startBar.getWholeBeats()), startBar.numerator);
        const auto startTime = ts.toTime(startBar);

        auto currentTime = startTime;
        auto prevBarX = -20.0;
        auto pixelsPerFrame = zoomState.durationToPixels(te::TimeDuration::fromSeconds(1.0 / fps));
        auto pixelsPerFrameTick = pixelsPerFrame;
        while (pixelsPerFrameTick < minPxPerDev) {
            pixelsPerFrameTick *= 2;
        }

        const auto end = zoomState.xToTime(width);
        while (currentTime <= end) {
            auto barBeats = ts.toBarsAndBeats(currentTime);

            auto nextDiv = BarsAndBeats { barBeats.bars, barBeats.beats + beatStep };
            auto nextTime = ts.toTime(nextDiv);
            auto pixelsPerDiv = zoomState.durationToPixels(nextTime - currentTime);
            if (pixelsPerDiv < minPxPerDev) {
                beatStep = beatStep * 2.0;
                continue;
            }

            auto x = static_cast<float>(zoomState.timeToX(currentTime));

            if (x >= 0) {
                if (barBeats.beats < te::BeatDuration::fromBeats(0.001)) {
                    g.setColour(Colors::Theme::borderLight);
                    if (x - prevBarX >= 20) {
                        // Bar line
                        g.drawLine(x, 0, x, height, 1.0f);
                        // Draw bar number
                        String barText = String(barBeats.bars + 1);
                        g.drawText(barText, Rectangle<float>(x + 2, -4, 20, 20), Justification::left);
                        prevBarX = x;
                    } else {
                        // smaller bar line
                        g.drawLine(x, 14, x, height, 1.0f);
                    }
                } else if (barBeats.getFractionalBeats() < te::BeatDuration::fromBeats(0.001)) {
                    // Beat line
                    g.setColour(Colors::Theme::border);
                    g.drawLine(x, height * 0.5f, x, height, 1.0f);
                } else {
                    // subdiv line
                    g.setColour(Colors::Theme::border);
                    g.drawLine(x, height * 0.66f, x, height, 1.0f);
                }
            }
            // draw frame ticks
            if (approximatelyEqual(pixelsPerFrame, pixelsPerFrameTick)) {
                auto frameTime = te::TimePosition::fromSeconds(std::ceil(currentTime.inSeconds() * fps) / fps);
                auto frameX = zoomState.timeToX(frameTime);
                auto nextX = zoomState.timeToX(nextTime);

                g.setColour(Colors::Theme::border.withAlpha(0.5f));
                for (float f = frameX; f < nextX; f += pixelsPerFrameTick) {
                    if (f > 0) {
                        g.drawLine(f, height * 0.75f, f, height, 1.0f);
                    }
                }
            }
            currentTime = nextTime;
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

    void repositionTransportToX(int x) {
        auto pos = editViewState.zoom.xToTime(x);
        edit.getTransport().setPosition(pos);
    }

private:
    te::Edit& edit;
    EditViewState& editViewState;
    juce::CachedValue<TimecodeDisplayFormatExt> timecodeFormat;

    void timerCallback() override {
        repaint();
    }

    // TempoSequenceChange listener implementation
    void selectableObjectChanged(te::Selectable* ) override {
        repaint();
    }

};

} // namespace MoTool
