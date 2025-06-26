#pragma once

#include <JuceHeader.h>
#include <memory>
#include <optional>

#include "../../controllers/EditState.h"
#include "../../models/PsgClip.h"
#include "juce_core/juce_core.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "tracktion_engine/tracktion_engine.h"


namespace MoTool {


class PsgParamList {
public:
    PsgParamList(const PsgClip& c, PsgParamType type)
        : clip(c)
        , paramType(type)
        , frames(clip.getPsg().getFrames())
    {}

    ~PsgParamList() {}

    int size() const {
        return frames.size();
    }

    float getMaxValue() const {
        // TODO implement in PsgParamFrame or PsgParamType
        return 4096.0f;
    }

    int findIndex(te::TimePosition pos) const {
        if (frames.isEmpty())
            return -1;

        int low = 0;
        int high = frames.size() - 1;

        while (low <= high) {
            int mid = (low + high) / 2;
            if (frames[mid]->getEditTime(clip) < pos)
                low = mid + 1;
            else
                high = mid - 1;
        }
        return low;
    }

    int findPrevActiveIndex(int idx) const {
        if (idx < 0 || idx >= frames.size())
            return 0;

        for (int i = idx; i >= 0; --i) {
            if (frames[i]->getData().isSet(paramType))
                return i;
        }
        return 0;
    }

    inline bool isActive(int idx) const {
        if (idx < 0 || idx >= frames.size())
            return false;
        return frames[idx]->getData().isSet(paramType);
    }

    inline te::TimePosition getTime(int idx) const {
        if (idx < 0 || idx >= frames.size())
            return te::TimePosition {};
        return frames[idx]->getEditTime(clip);
    }

    inline float getValue(int idx) const {
        if (idx < 0 || idx >= frames.size())
            return 0.0f;
        return frames[idx]->getData().getRaw(paramType) / getMaxValue();  // Normalize to 0.0 - 1.0 range
    }

    inline float getValueAt(te::TimePosition time) const {
        return getValue(findIndex(time));
    }

private:
    const PsgClip& clip;
    const PsgParamType paramType;
    const juce::Array<PsgParamFrame*>& frames;

    // juce::Array<EventType*>& getEventsChecked(juce::Array<EventType*>& events) {
    //     #if JUCE_DEBUG
    //         te::BeatPosition lastBeat;

    //         for (auto* e : events) {
    //             auto beat = e->getBeatPosition();
    //             jassert(lastBeat <= beat);
    //             lastBeat = beat;
    //         }
    //     #endif

    //     return events;
    // }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgParamList)
};


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

    void setCurrentClip(PsgClip* c) {
        currentClip = c;
        if (currentClip != nullptr) {
            paramList.emplace(*currentClip, currentParam);
        } else {
            paramList.reset();
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster* cb) override {
        if (cb == &editViewState.selectionManager) {
            if (auto* s = editViewState.selectionManager.getFirstItemOfType<PsgClip>()) {
                currentParam = PsgParamType::TonePeriodA;  // Default to TonePeriodA
                setCurrentClip(s);
            } else {
                setCurrentClip(nullptr);
            }
        }
        te::CurveEditor::changeListenerCallback(cb);
    }

    void selectableObjectChanged(te::Selectable* s) override {
        // TODO when is this called?
        if (auto psgClip = dynamic_cast<PsgClip*>(s)) {
            setCurrentClip(psgClip);
            currentParam = PsgParamType::TonePeriodA;
        } else {
            setCurrentClip(nullptr);
            currentParam = -1;
        }
        te::CurveEditor::selectableObjectChanged(s);  // updateLineThickness();
    }

    // CurveEditor overrides
    String getTooltip() override {
        return "PSG Parameters editor";
    }

    float getValue(int idx) const {
        return paramList->getValue(idx);
    }

    float getValueAt(te::TimePosition time) override {
        return paramList->getValueAt(time);
    }

    te::TimePosition getPointTime(int idx) override {
        return paramList->getTime(idx);
    }

    float getPointValue(int idx) override {
        return paramList->getValue(idx);
    }

    float getPointCurve(int /*idx*/) override {
        return -1.0f;
    }

    void removePoint(int /*index*/) override {
    }

    int addPoint(te::TimePosition /*time*/, float /*value*/, float /*curve*/) override {
        return 0;
    }

    int getNumPoints() override {
        return paramList.has_value() ? paramList->size() : 0;
    }

    te::CurvePoint getBezierHandle(int /*idx*/) override {
        return {};
    }

    te::CurvePoint getBezierPoint(int /*idx*/) override {
        return {};
    }

    int nextIndexAfter(te::TimePosition time) override {
        auto idx = paramList->findIndex(time);
        if (idx < 0 || idx >= paramList->size()) {
            return 0;  // No points found, return 0
        } else {
            return idx;
        }
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
        return Colours::white;
    }

    juce::Colour getCurveNameTextBackgroundColour() const override {
        return Colours::white;  // TODO color of the current PSG param
    }

    juce::Colour getPointOutlineColour() const override {
        return Colours::black;
    }

    double timeScale() const {
        // seconds per pixel
        return (rightTime - leftTime).inSeconds() / getWidth();
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
        auto backgroundColour = getBackgroundColour();

        g.fillAll(LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId));

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

        const int start = jmax(0, paramList->findPrevActiveIndex(paramList->findIndex(leftTime)));
        auto clipBounds = g.getClipBounds();
        {
            const int preStart = paramList->findPrevActiveIndex(start - 1);
            const auto startX = std::max(0.0f, timeToX({}));
            auto lastY = valueToY(getValue(preStart));
            juce::Path curvePath;
            curvePath.startNewSubPath(startX, lastY);
            curvePath.preallocateSpace((numPoints - start) * 2 + 2);

            // int counter = 0;
            auto p1 = getPosition(start);
            curvePath.lineTo(p1.getX(), lastY);
            curvePath.lineTo(p1);
            lastY = p1.y;
            for (int index = start; index < numPoints - 1; ++index) {

                if (!paramList->isActive(index + 1)) {
                    continue;
                }

                auto p2 = getPosition(index + 1);
                auto c = getPointCurve(index);

                if (c <= -1.0f) {
                    curvePath.lineTo(p2.getX(), lastY);
                    curvePath.lineTo(p2);
                } else if (c == 0.0f) {
                    curvePath.lineTo(p2);
                } else {
                    // auto bp = getPosition(getBezierPoint(index));

                    // if (c >= -0.5 && c <= 0.5) {
                    //     curvePath.quadraticTo(bp, p2);
                    // } else {
                    //     double lineX1, lineX2;
                    //     float lineY1, lineY2;
                    //     getBezierEnds(index, lineX1, lineY1, lineX2, lineY2);

                    //     curvePath.lineTo(getPosition({ TimePosition::fromSeconds(lineX1), lineY1 }));
                    //     curvePath.quadraticTo(bp, getPosition({ TimePosition::fromSeconds(lineX2), lineY2 }));
                    //     curvePath.lineTo(p2);
                    // }
                }

                lastY = p2.y;
                // ++counter;

                if (p2.x > clipBounds.getRight())
                    break;
            }

            // curvePath.lineTo((float) getWidth(), lastY);

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
            g.strokePath(curvePath, juce::PathStrokeType(lineThickness / 2));
        }

        //=========================================================================================
        // fast check if the scale allows drawing the points
        if (timeScale() > 1 / 50.0f / 4.0f) {
            return;
        }

        // draw the points along the line - the points, the add point and the curve point
        const bool anySelected = areAnyPointsSelected();

        if (isOver || isCurveSelected || anySelected) {
            juce::RectangleList<float> rects, selectedRects, fills;
            rects.ensureStorageAllocated(numPoints - start);

            if (anySelected)
                fills.ensureStorageAllocated(numPoints - start);

            // draw the white points
            for (int i = start; i < numPoints; ++i) {
                if (!paramList->isActive(i))
                    continue;
                auto pos = getPosition(i);

                juce::Rectangle<float> r(pos.x - pointRadius, pos.y - pointRadius,
                                         pointRadius * 2, pointRadius * 2);
                r = r.reduced(2);

                if (r.getX() > clipBounds.getRight())
                    break;

                const bool isSelected = isPointSelected(i);

                if (isSelected)
                    selectedRects.addWithoutMerging(r);
                else
                    rects.addWithoutMerging(r);

                if (!isSelected && i != pointUnderMouse)
                    fills.addWithoutMerging(r.reduced(1.0f));
            }

            g.setColour(getPointOutlineColour());
            g.fillRectList(rects);

            g.setColour(backgroundColour);
            g.fillRectList(fills);

            g.setColour(getSelectedLineColour());
            g.fillRectList(selectedRects);

            // // draw the curve points
            // for (int i = start; i < numPoints - 1; ++i) {
            //     auto pos = getPosition(getBezierHandle(i));

            //     juce::Rectangle<float> r (pos.x - pointRadius,
            //                             pos.y - pointRadius,
            //                             pointRadius * 2,
            //                             pointRadius * 2);
            //     r = r.reduced (2);

            //     if (r.getX() > clipBounds.getRight())
            //         break;

            //     g.setColour (curveColour);
            //     g.fillEllipse (r);

            //     if (i != curveUnderMouse && ! isPointSelected (i))
            //     {
            //         g.setColour (backgroundColour);
            //         g.fillEllipse (r.reduced (1.0f));
            //     }
            // }
        }
    }

bool hitTest(int x, int y) override {
    if (currentClip == nullptr)
        return false;
    auto py1 = valueToY(getValueAt(xToTime(x - 3.0f)));
    auto py2 = valueToY(getValueAt(xToTime(x + 3.0f)));

    if (y > std::min(py1, py2) - 4.0f && y < std::max(py1, py2) + 4.0f)
        return true;

    for (int i = firstIndexOnScreen; i < getNumPoints(); ++i) {
        auto t = getPointTime(i);

        if (t >= rightTime)
            break;

        auto px = timeToX(t);
        auto py = valueToY(getPointValue(i));

        if (std::abs(x - px) < pointRadius
             && std::abs(y - py) < pointRadius + 2)
            return true;
    }

    return false;
}

protected:
    void showBubbleForPointUnderMouse() override {
        DBG("showBubbleForPointUnderMouse");
    }

    void hideBubble() override {
        DBG("hideBubble");
    }

private:
    EditViewState& editViewState;
    PsgClip* currentClip = nullptr;
    PsgParamType currentParam;
    std::optional<PsgParamList> paramList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgParamEditorComponent)
};

}  // namespace MoTool