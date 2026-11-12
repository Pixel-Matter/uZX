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
    paramList_.setWantsKeyboardFocus(false);

    addAndMakeVisible(paramList_);
    addAndMakeVisible(editor_);

    // Listen to headersWidth changes
    editViewState_.state.addListener(this);

    // Select default parameter (NoiseIsOnA = row 3)
    paramList_.selectRow(3, false, false);
    editor_.setCurrentParam(PsgParamType(paramTypes_[3]));
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
    : editViewState(evs)
    , grid(g)
    , currentParam(PsgParamType::TonePeriodA)  // Initialize to match wrapper's default selection
{
    evs.zoom.addListener(this);
    grid.addListener(this);
    evs.selectionManager.addChangeListener(this);
}

PsgParamEditorComponent::~PsgParamEditorComponent() {
    editViewState.selectionManager.removeChangeListener(this);
    editViewState.zoom.removeListener(this);
    grid.removeListener(this);
}

void PsgParamEditorComponent::setCurrentParam(PsgParamType param) {
    currentParam = param;
    repaint();
}

void PsgParamEditorComponent::changeListenerCallback(ChangeBroadcaster*) {
    // Keeps showing the last PSG clip while something else is selected
    if (auto* s = editViewState.selectionManager.getFirstItemOfType<PsgClip>())
        currentClip = s;
    repaint();
}

void PsgParamEditorComponent::mouseDown(const MouseEvent& e) {
    if (auto* clip = currentClip.get())
        editViewState.selectionManager.select(clip, e.mods.isShiftDown());
}

float PsgParamEditorComponent::timeToX(te::TimePosition t) const {
    const auto range = editViewState.zoom.getRange();
    return (float) (getWidth() * ((t - range.getStart()) / range.getLength()));
}

te::TimePosition PsgParamEditorComponent::xToTime(float x) const {
    const auto range = editViewState.zoom.getRange();
    return te::TimePosition::fromSeconds(
        std::max(0.0, range.getStart().inSeconds() + x * range.getLength().inSeconds() / getWidth()));
}

float PsgParamEditorComponent::valueToY(float value) const {
    return 1.0f + (1.0f - value) * (getHeight() - 2);
}

Point<float> PsgParamEditorComponent::getPosition(const PsgParamList& params, int idx) const {
    return {timeToX(params.getTime(idx)), valueToY(params.getValue(idx))};
}

void PsgParamEditorComponent::paintGrid(Graphics& g) {
    g.fillAll(LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId));

    if (auto ticks = grid.getTicks(); !ticks.empty()) {
        MoToolApp::getApp().getLookAndFeel().drawTimelineGrid(g, getLocalBounds(), ticks);
    }
}

void PsgParamEditorComponent::paint(Graphics& g) {
    auto* clip = currentClip.get();
    const auto view = editViewState.zoom.getRange();
    if (clip == nullptr || view.isEmpty())
        return;

    const PsgParamList params(*clip, currentParam);
    const int numPoints = params.size();
    if (numPoints == 0)
        return;

    paintGrid(g);

    const bool highlighted = isMouseOverOrDragging() || editViewState.selectionManager.isSelected(clip);

    if (highlighted) {
        const String text(std::string(currentParam.getLabel()));
        g.setFont(13.0f);
        const auto tw = GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text);
        g.setColour(Colours::white);
        g.fillRect(8, 0, tw + 8, 16);
        g.setColour(Colours::black);
        g.drawText(text, 12, 0, tw + 6, 16, Justification::left, true);
    }

    const int start = jmax(0, params.findPrevActiveIndex(params.findIndex(view.getStart())));
    const auto clipBounds = g.getClipBounds();
    {
        const int preStart = params.findPrevActiveIndex(start - 1);
        const auto startX = std::max(0.0f, timeToX({}));
        auto lastY = valueToY(params.getValue(preStart));
        Path curvePath;
        curvePath.startNewSubPath(startX, lastY);
        curvePath.preallocateSpace((numPoints - start) * 2 + 2);

        auto p1 = getPosition(params, start);
        curvePath.lineTo(p1.getX(), lastY);
        curvePath.lineTo(p1);
        lastY = p1.y;
        for (int index = start; index < numPoints - 1; ++index) {
            if (!params.isActive(index + 1))
                continue;

            // A value holds until the next frame that sets it
            auto p2 = getPosition(params, index + 1);
            curvePath.lineTo(p2.getX(), lastY);
            curvePath.lineTo(p2);
            lastY = p2.y;

            if (p2.x > clipBounds.getRight())
                break;
        }

        g.setColour(Colours::white);
        g.strokePath(curvePath, PathStrokeType(highlighted ? 1.0f : 0.5f));
    }

    // Points only when frames are far enough apart: under 1/200 s per pixel
    if (!highlighted || view.getLength().inSeconds() / getWidth() > 1 / 50.0f / 4.0f)
        return;

    RectangleList<float> rects, fills;
    rects.ensureStorageAllocated(numPoints - start);
    fills.ensureStorageAllocated(numPoints - start);

    for (int i = start; i < numPoints; ++i) {
        if (!params.isActive(i))
            continue;
        auto pos = getPosition(params, i);
        auto r = Rectangle<float>(pos.x - pointRadius, pos.y - pointRadius, pointRadius * 2, pointRadius * 2).reduced(2);

        if (r.getX() > clipBounds.getRight())
            break;

        rects.addWithoutMerging(r);
        fills.addWithoutMerging(r.reduced(1.0f));
    }

    g.setColour(Colours::black);
    g.fillRectList(rects);
    g.setColour(Colours::white);
    g.fillRectList(fills);
}

bool PsgParamEditorComponent::hitTest(int x, int y) {
    auto* clip = currentClip.get();
    if (clip == nullptr)
        return false;

    const PsgParamList params(*clip, currentParam);
    auto py1 = valueToY(params.getValueAt(xToTime(x - 3.0f)));
    auto py2 = valueToY(params.getValueAt(xToTime(x + 3.0f)));

    if (y > std::min(py1, py2) - 4.0f && y < std::max(py1, py2) + 4.0f)
        return true;

    const auto end = editViewState.zoom.getRange().getEnd();
    for (int i = jmax(0, params.findIndex(xToTime(x - pointRadius))); i < params.size(); ++i) {
        if (params.getTime(i) >= end)
            break;

        auto p = getPosition(params, i);
        if (std::abs(x - p.x) < pointRadius && std::abs(y - p.y) < pointRadius + 2)
            return true;
    }

    return false;
}

}  // namespace MoTool
