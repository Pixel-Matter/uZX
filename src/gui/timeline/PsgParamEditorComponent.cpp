#include "PsgParamEditorComponent.h"
#include "../../controllers/App.h"

namespace MoTool {

// PsgParamList implementation
PsgParamList::PsgParamList(const PsgClip& c, PsgParamType type)
    : clip(c)
    , paramType(type)
    , frames(clip.getPsg().getFrames())
{
}

PsgParamList::~PsgParamList() {
}

int PsgParamList::size() const {
    return frames.size();
}

float PsgParamList::getMaxValue() const {
    // TODO implement in PsgParamFrame or PsgParamType
    return 4096.0f;
}

int PsgParamList::findIndex(te::TimePosition pos) const {
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

int PsgParamList::findPrevActiveIndex(int idx) const {
    if (idx < 0 || idx >= frames.size())
        return 0;

    for (int i = idx; i >= 0; --i) {
        if (frames[i]->getData().isSet(paramType))
            return i;
    }
    return 0;
}

bool PsgParamList::isActive(int idx) const {
    if (idx < 0 || idx >= frames.size())
        return false;
    return frames[idx]->getData().isSet(paramType);
}

te::TimePosition PsgParamList::getTime(int idx) const {
    if (idx < 0 || idx >= frames.size())
        return te::TimePosition {};
    return frames[idx]->getEditTime(clip);
}

float PsgParamList::getValue(int idx) const {
    if (idx < 0 || idx >= frames.size())
        return 0.0f;
    return frames[idx]->getData().getRaw(paramType) / getMaxValue();  // Normalize to 0.0 - 1.0 range
}

float PsgParamList::getValueAt(te::TimePosition time) const {
    return getValue(findIndex(time));
}

// PsgParamEditorComponent implementation
PsgParamEditorComponent::PsgParamEditorComponent(EditViewState& evs, TimelineGrid& g)
    : CurveEditor(evs.edit, evs.selectionManager)
    , editViewState(evs)
    , grid(g)
{
    setTimes(editViewState.zoom.getRange().getStart(), editViewState.zoom.getRange().getEnd());
    evs.zoom.addListener(this);
}

PsgParamEditorComponent::~PsgParamEditorComponent() {
    editViewState.zoom.removeListener(this);
}

void PsgParamEditorComponent::zoomChanged() {
    setTimes(editViewState.zoom.getRange().getStart(), editViewState.zoom.getRange().getEnd());
    repaint();
}

void PsgParamEditorComponent::setCurrentClip(PsgClip* c) {
    currentClip = c;
    if (currentClip.get() != nullptr) {
        paramList.emplace(*currentClip, currentParam);
    } else {
        paramList.reset();
    }
}

void PsgParamEditorComponent::changeListenerCallback(juce::ChangeBroadcaster* cb) {
    if (cb == &editViewState.selectionManager) {
        if (auto* s = editViewState.selectionManager.getFirstItemOfType<PsgClip>()) {
            currentParam = PsgParamType::TonePeriodA;  // Default to TonePeriodA
            setCurrentClip(s);
        }
        // else {
        //     setCurrentClip(nullptr);
        // }
    }
    te::CurveEditor::changeListenerCallback(cb);
}

void PsgParamEditorComponent::selectableObjectChanged(te::Selectable* s) {
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

String PsgParamEditorComponent::getTooltip() {
    return "PSG Parameters editor";
}

float PsgParamEditorComponent::getValue(int idx) const {
    return paramList->getValue(idx);
}

float PsgParamEditorComponent::getValueAt(te::TimePosition time) {
    return paramList->getValueAt(time);
}

te::TimePosition PsgParamEditorComponent::getPointTime(int idx) {
    return paramList->getTime(idx);
}

float PsgParamEditorComponent::getPointValue(int idx) {
    return paramList->getValue(idx);
}

float PsgParamEditorComponent::getPointCurve(int /*idx*/) {
    return -1.0f;
}

void PsgParamEditorComponent::removePoint(int /*index*/) {
}

int PsgParamEditorComponent::addPoint(te::TimePosition /*time*/, float /*value*/, float /*curve*/) {
    return 0;
}

int PsgParamEditorComponent::getNumPoints() {
    return paramList.has_value() ? paramList->size() : 0;
}

te::CurvePoint PsgParamEditorComponent::getBezierHandle(int /*idx*/) {
    return {};
}

te::CurvePoint PsgParamEditorComponent::getBezierPoint(int /*idx*/) {
    return {};
}

int PsgParamEditorComponent::nextIndexAfter(te::TimePosition time) {
    auto idx = paramList->findIndex(time);
    if (idx < 0 || idx >= paramList->size()) {
        return 0;  // No points found, return 0
    } else {
        return idx;
    }
}

void PsgParamEditorComponent::getBezierEnds(int /*index*/, double& x1out, float& y1out, double& x2out, float& y2out) {
    x1out = 0;
    y1out = 0;
    x2out = 0;
    y2out = 0;
}

int PsgParamEditorComponent::movePoint(int /*index*/, te::TimePosition /*newTime*/, float /*newValue*/, bool /*removeInterveningPoints*/) {
    return 0;
}

void PsgParamEditorComponent::setValueWhenNoPoints(float /*value*/) {
    // If no points add one anyway
}

te::CurveEditorPoint* PsgParamEditorComponent::createPoint(int /*idx*/) {
    return nullptr;
}

int PsgParamEditorComponent::curvePoint(int /*index*/, float /*newCurve*/) {
    return 0;
}

juce::String PsgParamEditorComponent::getCurveName() {
    if (currentClip.get() != nullptr) {
        return std::string(currentParam.getLabel());
    }
    return {};
}

int PsgParamEditorComponent::getCurveNameOffset() {
    return 0;
}

te::Selectable* PsgParamEditorComponent::getItem() {
    return currentClip.get();
}

bool PsgParamEditorComponent::isShowingCurve() const {
    return currentClip.get() != nullptr;
}

void PsgParamEditorComponent::updateFromTrack() {
    // TODO
}

juce::Colour PsgParamEditorComponent::getCurrentLineColour() {
    return Colours::white;
}

juce::Colour PsgParamEditorComponent::getCurrentFillColour() {
    return juce::Colours::transparentBlack;
}

juce::Colour PsgParamEditorComponent::getDefaultLineColour() const {
    return Colours::black;
}

juce::Colour PsgParamEditorComponent::getSelectedLineColour() const {
    return Colours::yellow;
}

juce::Colour PsgParamEditorComponent::getBackgroundColour() const {
    return Colours::white;
}

juce::Colour PsgParamEditorComponent::getCurveNameTextBackgroundColour() const {
    return Colours::white;  // TODO color of the current PSG param
}

juce::Colour PsgParamEditorComponent::getPointOutlineColour() const {
    return Colours::black;
}

double PsgParamEditorComponent::timeScale() const {
    // seconds per pixel
    return (rightTime - leftTime).inSeconds() / getWidth();
}

void PsgParamEditorComponent::paintGrid(juce::Graphics& g) {
    g.fillAll(LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId));

    if (auto ticks = grid.getTicks(); !ticks.empty()) {
        MoToolApp::getApp().getLookAndFeel().drawTimelineGrid(g, getLocalBounds(), ticks);
    }
}

void PsgParamEditorComponent::paint(juce::Graphics& g) {
    using namespace tracktion;
    CRASH_TRACER

    if ((rightTime - leftTime) == TimeDuration())
        return;

    const int numPoints = getNumPoints();
    if (numPoints == 0) {
        return;
    }

    paintGrid(g);

    const bool isOver = isMouseOverOrDragging();
    // auto curveColour = getCurrentLineColour();
    auto backgroundColour = getBackgroundColour();

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

bool PsgParamEditorComponent::hitTest(int x, int y) {
    if (currentClip.get() == nullptr)
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

void PsgParamEditorComponent::showBubbleForPointUnderMouse() {
    DBG("showBubbleForPointUnderMouse");
}

void PsgParamEditorComponent::hideBubble() {
    DBG("hideBubble");
}

}  // namespace MoTool