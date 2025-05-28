#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../../controllers/EditState.h"
#include "../../models/PsgClip.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "tracktion_engine/tracktion_engine.h"


namespace MoTool {

class PsgParamEditorComponent: public te::CurveEditor,
                               public ZoomViewState::Listener
{
public:
    PsgParamEditorComponent(EditViewState& evs)
        : CurveEditor(evs.edit, evs.selectionManager)
        , editViewState(evs)
    {
        setTimes(editViewState.zoom.getRange().getStart(), editViewState.zoom.getRange().getEnd());
        evs.zoom.addListener(this);
    }

    ~PsgParamEditorComponent() override {
        editViewState.zoom.removeListener(this);
    }

    // ZoomViewState::Listener overrides
    void zoomChanged() override {
        setTimes(editViewState.zoom.getRange().getStart(), editViewState.zoom.getRange().getEnd());
        repaint();
    }

    void changeListenerCallback(juce::ChangeBroadcaster* cb) override {
        if (cb == &editViewState.selectionManager) {
            if (auto* s = editViewState.selectionManager.getFirstItemOfType<PsgClip>()) {
                currentClip = s;
                currentParam = PsgParamType::TonePeriodA;  // Default to TonePeriodA
            } else {
                currentClip = nullptr;
            }
        }
        te::CurveEditor::changeListenerCallback(cb);
    }

    void selectableObjectChanged(te::Selectable* s) override {
        if (auto psgClip = dynamic_cast<PsgClip*>(s)) {
            currentParam = PsgParamType::TonePeriodA;
            // currentParam = psgClip->getCurrentParam();
            paramFrames = psgClip->getPsg().getFrames();
        } else {
            currentClip = nullptr;
            currentParam = -1;
            paramFrames.clear();
        }
        te::CurveEditor::selectableObjectChanged(s);  // updateLineThickness();
    }

    // CurveEditor overrides
    String getTooltip() override {
        return "PSG Parameters editor";
    }

    float getMaxValue() {
        return 4096.0f;
    }

    float getValueAt(te::TimePosition time) override {
        if (currentClip != nullptr) {
            auto& frames = currentClip->getPsg().getFrames();
            for (const auto& frame : frames) {
                if (frame->getEditTime(*currentClip) >= time) {
                    return frame->getData().getRaw(currentParam) / getMaxValue();  // Normalize to 0.0 - 1.0 range
                }
            }
        }
        return 0.0f;  // Default value if no frame found at the given time
    }

    te::TimePosition getPointTime(int idx) override {
        if (currentClip != nullptr) {
            auto& frames = currentClip->getPsg().getFrames();
            return frames[idx]->getEditTime(*currentClip);
        }
        return te::TimePosition();
    }

    float getPointValue(int idx) override {
        if (currentClip != nullptr) {
            auto& frames = currentClip->getPsg().getFrames();
            return frames[idx]->getData().getRaw(currentParam) / getMaxValue();  // Normalize to 0.0 - 1.0 range
        }
        return 0.0f;
    }

    float getPointCurve(int /*idx*/) override {
        return 0;
    }

    void removePoint(int /*index*/) override {

    }

    int addPoint(te::TimePosition /*time*/, float /*value*/, float /*curve*/) override {
        return 0;
    }

    int getNumPoints() override {
        if (currentClip != nullptr) {
            return currentClip->getPsg().getFrames().size();
        }
        return 0;
    }

    te::CurvePoint getBezierHandle(int /*idx*/) override {
        return {};
    }

    te::CurvePoint getBezierPoint(int /*idx*/) override {
        return {};
    }

    int nextIndexAfter(te::TimePosition time) override {
        if (currentClip != nullptr) {
            auto& frames = currentClip->getPsg().getFrames();
            for (int i = 0; i < frames.size(); ++i) {
                if (frames[i]->getEditTime(*currentClip) > time) {
                    return i;
                }
            }
        }
        return 0;
    }

    void getBezierEnds(int /*index*/, double& x1out, float& y1out, double& x2out, float& y2out) override {
        x1out = 0;
        y1out = 0;
        x2out = 0;
        y2out = 0;
    }

    int movePoint(int /*index*/, te::TimePosition /*newTime*/, float /*newValue*/, bool /*removeInterveningPoints*/) override {
        return 0;
    }

    void setValueWhenNoPoints(float /*value*/) override {
        // If no points add one anyway
    }

    te::CurveEditorPoint* createPoint(int /*idx*/) override {
        return nullptr;
    }

    int curvePoint(int /*index*/, float /*newCurve*/) override {
        return 0;
    }

    juce::String getCurveName() override {
        if (currentClip != nullptr) {
            return std::string(currentParam.getLabel());
        }
        return {};
    }

    int getCurveNameOffset() override {
        return 0;
    }

    te::Selectable* getItem() override {
        return currentClip;
    }

    // not used yet
    bool isShowingCurve() const override {
        return currentClip != nullptr;
    }

    void updateFromTrack() override {
        // TODO
    }

    juce::Colour getCurrentLineColour() override {
        return Colours::white;
    }

    juce::Colour getCurrentFillColour() override {
        return juce::Colours::transparentBlack;
    }

    juce::Colour getDefaultLineColour() const override {
        return Colours::black;
    }

    juce::Colour getSelectedLineColour() const override {
        return Colours::yellow;
    }

    juce::Colour getBackgroundColour() const override {
        return getLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    }

    juce::Colour getCurveNameTextBackgroundColour() const override {
        return Colours::white;  // TODO color of the current PSG param
    }

    juce::Colour getPointOutlineColour() const override {
        return Colours::black;
    }

    void paint(juce::Graphics& g) override {
        using namespace tracktion;
        CRASH_TRACER

        if ((rightTime - leftTime) == TimeDuration())
            return;

        const int numPoints = getNumPoints();
        if (numPoints == 0) {
            return;
        }

        const bool isOver = isMouseOverOrDragging();
        // auto curveColour = getCurrentLineColour();
        // auto backgroundColour = getBackgroundColour();

        // draw the name of the curve
        if (isOver || isCurveSelected) {
            g.setColour(getCurveNameTextBackgroundColour());
            auto text = getCurveName();
            g.setFont(13.0f);
        #if JUCE_MAJOR_VERSION >= 8
            auto tw = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text);
        #else
            auto tw = g.getCurrentFont().getStringWidth (text);
        #endif
            auto tx = getCurveNameOffset() + 8;
            g.fillRect(tx, 0, tw + 8, 16);

            g.setColour(getDefaultLineColour());
            g.drawText(text, tx + 4, 0, tw + 6, 16, juce::Justification::left, true);
        }

        // draw the line to the first point, or all the way across if there are no points
        const int start = std::max(0, nextIndexAfter(leftTime) - 1);
        DBG("CurveEditor::paint cp3 start: " << start);

        g.fillAll(LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId));

        auto clipBounds = g.getClipBounds();

        {
            const auto startX = std::max(0.0f, timeToX({}));
            DBG("CurveEditor::paint cp4 startX: " << startX);
            auto lastY = valueToY(getValueAt(leftTime));

            juce::Path curvePath;
            curvePath.startNewSubPath(startX, lastY);
            curvePath.preallocateSpace((numPoints - start) * 5 + 1);

            int counter = 0;
            for (int index = start; index < numPoints - 1; ++index) {
                if (index == start)
                    curvePath.lineTo(getPosition(index));

                auto p2 = getPosition(index + 1);
                auto c = getPointCurve(index);

                if (c == 0) {
                    curvePath.lineTo(p2);
                } else {
                    auto bp = getPosition(getBezierPoint(index));

                    if (c >= -0.5 && c <= 0.5) {
                        curvePath.quadraticTo(bp, p2);
                    } else {
                        double lineX1, lineX2;
                        float lineY1, lineY2;
                        getBezierEnds(index, lineX1, lineY1, lineX2, lineY2);

                        curvePath.lineTo(getPosition({ TimePosition::fromSeconds(lineX1), lineY1 }));
                        curvePath.quadraticTo(bp, getPosition({ TimePosition::fromSeconds(lineX2), lineY2 }));
                        curvePath.lineTo(p2);
                    }
                }

                lastY = p2.y;
                ++counter;

                if (p2.x > clipBounds.getRight())
                    break;
            }
            DBG("CurveEditor::paint cp5 points: " << counter);

            curvePath.lineTo((float) getWidth(), lastY);

            if (auto fillCol = getCurrentFillColour(); ! fillCol.isTransparent()) {
                juce::Path fillPath(curvePath);
                const auto y = getHeight() + 1.0f;
                fillPath.lineTo((float) getWidth(), y);
                fillPath.lineTo(startX, y);
                fillPath.closeSubPath();

                g.setColour(fillCol);
                g.fillPath(fillPath);
            }

            g.setColour(getCurrentLineColour());
            g.strokePath(curvePath, juce::PathStrokeType(lineThickness));
        }

        // draw the points along the line - the points, the add point and the curve point
        const bool anySelected = areAnyPointsSelected();

        // if (isOver || isCurveSelected || anySelected)
        // {
        //     juce::RectangleList<float> rects, selectedRects, fills;
        //     rects.ensureStorageAllocated (numPoints);

        //     if (anySelected)
        //         fills.ensureStorageAllocated (numPoints);

        //     // draw the white points
        //     for (int i = start; i < numPoints; ++i)
        //     {
        //         auto pos = getPosition (i);

        //         juce::Rectangle<float> r (pos.x - pointRadius,
        //                                 pos.y - pointRadius,
        //                                 pointRadius * 2,
        //                                 pointRadius * 2);
        //         r = r.reduced (2);

        //         if (r.getX() > clipBounds.getRight())
        //             break;

        //         const bool isSelected = isPointSelected (i);

        //         if (isSelected)
        //             selectedRects.addWithoutMerging (r);
        //         else
        //             rects.addWithoutMerging (r);

        //         if (! isSelected && i != pointUnderMouse)
        //             fills.addWithoutMerging (r.reduced (1.0f));
        //     }

        //     g.setColour (getPointOutlineColour());
        //     g.fillRectList (rects);

        //     g.setColour (backgroundColour);
        //     g.fillRectList (fills);

        //     g.setColour (getSelectedLineColour());
        //     g.fillRectList (selectedRects);

        //     // draw the curve points
        //     for (int i = start; i < numPoints - 1; ++i)
        //     {
        //         auto pos = getPosition (getBezierHandle (i));

        //         juce::Rectangle<float> r (pos.x - pointRadius,
        //                                 pos.y - pointRadius,
        //                                 pointRadius * 2,
        //                                 pointRadius * 2);
        //         r = r.reduced (2);

        //         if (r.getX() > clipBounds.getRight())
        //             break;

        //         g.setColour (curveColour);
        //         g.fillEllipse (r);

        //         if (i != curveUnderMouse && ! isPointSelected (i))
        //         {
        //             g.setColour (backgroundColour);
        //             g.fillEllipse (r.reduced (1.0f));
        //         }
        //     }
        // }
    }

protected:
    void showBubbleForPointUnderMouse() override {
    }

    void hideBubble() override {
    }

private:
    EditViewState& editViewState;
    PsgClip* currentClip = nullptr;
    PsgParamType currentParam;
    juce::Array<PsgParamFrame*> paramFrames;
};

}  // namespace MoTool