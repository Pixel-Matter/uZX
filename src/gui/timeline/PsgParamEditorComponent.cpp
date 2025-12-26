#include "PsgParamEditorComponent.h"
#include "../../controllers/App.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

//==============================================================================
// PsgParamEditorWrapper implementation
//==============================================================================
PsgParamEditorWrapper::PsgParamEditorWrapper(EditViewState& evs, TimelineGrid& g)
    : editViewState_(evs)
    , editor_(evs, g)
{
    // Initialize parameter types - most commonly used parameters
    paramTypes_ = {
        PsgParamType::VolumeA,
        PsgParamType::TonePeriodA,
        PsgParamType::ToneIsOnA,
        PsgParamType::NoiseIsOnA,
        PsgParamType::EnvelopeIsOnA,
        PsgParamType::VolumeB,
        PsgParamType::TonePeriodB,
        PsgParamType::ToneIsOnB,
        PsgParamType::NoiseIsOnB,
        PsgParamType::EnvelopeIsOnB,
        PsgParamType::VolumeC,
        PsgParamType::TonePeriodC,
        PsgParamType::ToneIsOnC,
        PsgParamType::NoiseIsOnC,
        PsgParamType::EnvelopeIsOnC,
        PsgParamType::NoisePeriod,
        PsgParamType::EnvelopePeriod,
        PsgParamType::EnvelopeShape,
        PsgParamType::RetriggerEnvelope
    };

    paramList_.setModel(this);
    paramList_.setMultipleSelectionEnabled(false);
    paramList_.setRowHeight(20);
    paramList_.setOutlineThickness(0);

    addAndMakeVisible(paramList_);
    addAndMakeVisible(editor_);

    // Listen to headersWidth changes
    editViewState_.state.addListener(this);

    // Select first parameter by default (TonePeriodA)
    paramList_.selectRow(3, false, false);
}

PsgParamEditorWrapper::~PsgParamEditorWrapper() {
    editViewState_.state.removeListener(this);
}

int PsgParamEditorWrapper::getNumRows() {
    return static_cast<int>(paramTypes_.size());
}

void PsgParamEditorWrapper::paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) {
    if (rowNumber < 0 || rowNumber >= static_cast<int>(paramTypes_.size()))
        return;

    PsgParamType param(paramTypes_[rowNumber]);
    auto label = param.getLabel();

    if (rowIsSelected) {
        g.setColour(Colors::Theme::primary);
        g.fillRect(0, 0, width, height);
    }

    // Text
    g.setColour(Colors::Theme::textPrimary);
    g.drawText(String(label.data(), label.size()), 4, 0, width - 8, height, Justification::centredLeft, true);
}

void PsgParamEditorWrapper::listBoxItemClicked(int row, const MouseEvent&) {
    if (row >= 0 && row < static_cast<int>(paramTypes_.size())) {
        editor_.setCurrentParam(PsgParamType(paramTypes_[row]));
    }
}

void PsgParamEditorWrapper::paint(Graphics& g) {
    // No need to paint background - ListBox handles it
}

void PsgParamEditorWrapper::resized() {
    auto bounds = getLocalBounds();
    const int headerWidth = editViewState_.getTrackHeaderWidth();

    // Get tab bar width from parent TabbedComponent
    int tabBarWidth = 0;
    if (!tabbedComponent_) {
        // Find TabbedComponent parent on first resize
        auto* parent = getParentComponent();
        while (parent) {
            if (auto* tabbed = dynamic_cast<TabbedComponent*>(parent)) {
                tabbedComponent_ = tabbed;
                break;
            }
            parent = parent->getParentComponent();
        }
    }

    if (tabbedComponent_) {
        tabBarWidth = tabbedComponent_->getTabBarDepth();
    }

    // Left area for parameter selector: headerWidth - tabBarWidth - 8px
    const int listWidth = headerWidth - tabBarWidth - 8;
    auto leftArea = bounds.removeFromLeft(listWidth);
    paramList_.setBounds(leftArea);

    // Right area for the editor
    editor_.setBounds(bounds);
}

void PsgParamEditorWrapper::valueTreePropertyChanged(ValueTree& tree, const Identifier& property) {
    if (property == IDs::headersWidth && tree.hasType(IDs::EDITVIEWSTATE)) {
        resized();
    }
}

//==============================================================================

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

ParameterScale PsgParamList::getScale() const {
    return paramType.getScale();
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
    return paramType.valueToNormalized(frames[idx]->getData().getRaw(paramType));
}

float PsgParamList::getValueAt(te::TimePosition time) const {
    return getValue(findIndex(time));
}

// PsgParamEditorComponent implementation
PsgParamEditorComponent::PsgParamEditorComponent(EditViewState& evs, TimelineGrid& g)
    : CurveEditor(evs.edit, evs.selectionManager)
    , editViewState(evs)
    , grid(g)
    , currentParam(PsgParamType::TonePeriodA)  // Initialize to match wrapper's default selection
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

void PsgParamEditorComponent::setCurrentParam(PsgParamType param) {
    currentParam = param;
    if (currentClip.get() != nullptr) {
        paramList.emplace(*currentClip, currentParam);
    }
    repaint();
}

void PsgParamEditorComponent::changeListenerCallback(ChangeBroadcaster* cb) {
    if (cb == &editViewState.selectionManager) {
        if (auto* s = editViewState.selectionManager.getFirstItemOfType<PsgClip>()) {
            // Keep the current parameter selection when switching clips
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
        // Keep the current parameter selection when switching clips
        setCurrentClip(psgClip);
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

float PsgParamEditorComponent::getValueAt(te::EditPosition time) {
    return paramList->getValueAt(tracktion::toTime(time, edit.tempoSequence));
}

te::EditPosition PsgParamEditorComponent::getPointPosition(int idx) {
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

int PsgParamEditorComponent::addPoint(te::EditPosition /*time*/, float /*value*/, float /*curve*/) {
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

int PsgParamEditorComponent::nextIndexAfter(te::EditPosition time) {
    auto idx = paramList->findIndex(tracktion::toTime(time, edit.tempoSequence));
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

std::pair<te::CurvePoint, te::CurvePoint> PsgParamEditorComponent::getBezierEnds(int /*index*/) {
    return { {}, {} };
}

int PsgParamEditorComponent::movePoint(int /*index*/, te::EditPosition /*newTime*/, float /*newValue*/, bool /*removeInterveningPoints*/) {
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

String PsgParamEditorComponent::getCurveName() {
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

Colour PsgParamEditorComponent::getCurrentLineColour() {
    return Colours::white;
}

Colour PsgParamEditorComponent::getCurrentFillColour() {
    return Colours::transparentBlack;
}

Colour PsgParamEditorComponent::getDefaultLineColour() const {
    return Colours::black;
}

Colour PsgParamEditorComponent::getSelectedLineColour() const {
    return Colours::yellow;
}

Colour PsgParamEditorComponent::getBackgroundColour() const {
    return Colours::white;
}

Colour PsgParamEditorComponent::getCurveNameTextBackgroundColour() const {
    return Colours::white;  // TODO color of the current PSG param
}

Colour PsgParamEditorComponent::getPointOutlineColour() const {
    return Colours::black;
}

Range<float> PsgParamEditorComponent::getParameterRange() const {
    return Range<float>(0.0f, 1.0f);
}

double PsgParamEditorComponent::timeScale() const {
    // seconds per pixel
    auto leftTimePos = tracktion::toTime(leftTime, edit.tempoSequence);
    auto rightTimePos = tracktion::toTime(rightTime, edit.tempoSequence);
    return (rightTimePos - leftTimePos).inSeconds() / getWidth();
}

void PsgParamEditorComponent::paintGrid(Graphics& g) {
    g.fillAll(LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId));

    if (auto ticks = grid.getTicks(); !ticks.empty()) {
        MoToolApp::getApp().getLookAndFeel().drawTimelineGrid(g, getLocalBounds(), ticks);
    }
}

void PsgParamEditorComponent::paint(Graphics& g) {
    using namespace tracktion;
    CRASH_TRACER

    auto leftTimePos = tracktion::toTime(leftTime, edit.tempoSequence);
    auto rightTimePos = tracktion::toTime(rightTime, edit.tempoSequence);
    if ((rightTimePos - leftTimePos) == tracktion::TimeDuration())
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
        auto tw = GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text);
    #else
        auto tw = g.getCurrentFont().getStringWidth (text);
    #endif
        auto tx = getCurveNameOffset() + 8;
        g.fillRect(tx, 0, tw + 8, 16);

        g.setColour(getDefaultLineColour());
        g.drawText(text, tx + 4, 0, tw + 6, 16, Justification::left, true);
    }

    const int start = jmax(0, paramList->findPrevActiveIndex(paramList->findIndex(leftTimePos)));
    auto clipBounds = g.getClipBounds();
    {
        const int preStart = paramList->findPrevActiveIndex(start - 1);
        const auto startX = std::max(0.0f, timeToX({}));
        auto lastY = valueToY(getValue(preStart));
        Path curvePath;
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
            Path fillPath(curvePath);
            const auto y = getHeight() + 1.0f;
            fillPath.lineTo((float) getWidth(), y);
            fillPath.lineTo(startX, y);
            fillPath.closeSubPath();

            g.setColour(fillCol);
            g.fillPath(fillPath);
        }

        g.setColour(getCurrentLineColour());
        g.strokePath(curvePath, PathStrokeType(lineThickness / 2));
    }

    //=========================================================================================
    // fast check if the scale allows drawing the points
    if (timeScale() > 1 / 50.0f / 4.0f) {
        return;
    }

    // draw the points along the line - the points, the add point and the curve point
    const bool anySelected = areAnyPointsSelected();

    if (isOver || isCurveSelected || anySelected) {
        RectangleList<float> rects, selectedRects, fills;
        rects.ensureStorageAllocated(numPoints - start);

        if (anySelected)
            fills.ensureStorageAllocated(numPoints - start);

        // draw the white points
        for (int i = start; i < numPoints; ++i) {
            if (!paramList->isActive(i))
                continue;
            auto pos = getPosition(i);

            Rectangle<float> r(pos.x - pointRadius, pos.y - pointRadius,
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

        //     Rectangle<float> r (pos.x - pointRadius,
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
        auto t = getPointPosition(i);

        if (tracktion::greaterThanOrEqualTo(t, rightTime, edit.tempoSequence))
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