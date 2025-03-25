#pragma once

#include <JuceHeader.h>

#include "../common/LookAndFeel.h"
#include "../common/Components.h"
#include "../common/Utilities.h"
#include "../../model/Timecode.h"


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
        // cached value is ValueTree::Listener
        timecodeFormat.referTo(edit.state, te::IDs::timecodeFormat, nullptr, TimecodeDisplayFormatExt {TimecodeTypeExt::barsBeatsFps50});
        // startTimerHz(30);
    }

    ~RulerComponent() override {
        edit.tempoSequence.removeListener(this);
        editViewState.state.removeListener(this);
        stopTimer();
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

        auto startBar = ts.toBarsAndBeats(zoomState.getRangeStart());
        startBar = te::tempo::BarsAndBeats(startBar.bars, te::BeatDuration::fromBeats(startBar.getWholeBeats()), startBar.numerator);
        const auto startTime = ts.toTime(startBar);

        auto currentTime = startTime;
        auto prevBarX = -20.0;
        auto pixelsPerFrame = zoomState.durationToPixels(te::TimeDuration::fromSeconds(1.0 / fps), width);
        auto pixelsPerFrameTick = pixelsPerFrame;
        while (pixelsPerFrameTick < minPxPerDev) {
            pixelsPerFrameTick *= 2;
        }

        while (currentTime <= zoomState.getRangeEnd()) {
            auto barBeats = ts.toBarsAndBeats(currentTime);

            auto nextDiv = BarsAndBeats { barBeats.bars, barBeats.beats + beatStep };
            auto nextTime = ts.toTime(nextDiv);
            auto pixelsPerDiv = zoomState.durationToPixels(nextTime - currentTime, width);
            if (pixelsPerDiv < minPxPerDev) {
                beatStep = beatStep * 2.0;
                continue;
            }

            auto x = static_cast<float>(zoomState.timeToX(currentTime, width));

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
            if (exactlyEqual(pixelsPerFrame, pixelsPerFrameTick)) {
                auto frameTime = te::TimePosition::fromSeconds(std::ceil(currentTime.inSeconds() * fps) / fps);
                auto frameX = static_cast<float>(zoomState.timeToX(frameTime, width));
                auto nextX = static_cast<float>(zoomState.timeToX(nextTime, width));

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
            m.addItem("Zoom In", [this] {
                // TODO link to command
                editViewState.zoom.zoomHorizontally(1.0 / 1.25);
            });
            m.addItem("Zoom Out", [this] {
                // TODO link to command
                editViewState.zoom.zoomHorizontally(1.25);
            });
            m.addItem("Zoom Fit", [this] {
                // TODO link to command
                auto range = Helpers::getEffectiveClipsTimeRange(edit);
                if (!range.isEmpty()) {
                    editViewState.zoom.setRange(range);
                }
            });
            m.showMenuAsync({});
        } else {
            repositionTransportToX(e.x);
        }
    }

    void repositionTransportToX(int x) {
        auto pos = editViewState.zoom.xToTime(x, getWidth());
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

    void valueTreePropertyChanged(ValueTree&, const Identifier& prop) override {
        if (prop == IDs::viewX1 || prop == IDs::viewX2) {
            repaint();
        }
    }
};

} // namespace MoTool
