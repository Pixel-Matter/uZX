#pragma once

#include <JuceHeader.h>
#include <optional>

#include "TimelineGrid.h"
#include "../../controllers/EditState.h"
#include "../../models/PsgClip.h"


namespace MoTool {


class PsgParamList {
public:
    PsgParamList(const PsgClip& c, PsgParamType type);
    ~PsgParamList();

    int size() const;
    ParameterScale getScale() const;
    int findIndex(te::TimePosition pos) const;
    int findPrevActiveIndex(int idx) const;
    bool isActive(int idx) const;
    te::TimePosition getTime(int idx) const;
    float getValue(int idx) const;
    float getValueAt(te::TimePosition time) const;

private:
    const PsgClip& clip;
    const PsgParamType paramType;
    const Array<PsgParamFrame*>& frames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgParamList)
};


class PsgParamEditorComponent: public te::CurveEditor,
                               public ZoomViewState::Listener,
                               private TimelineGrid::Listener
{
public:
    PsgParamEditorComponent(EditViewState& evs, TimelineGrid& g);
    ~PsgParamEditorComponent() override;

    void setCurrentParam(PsgParamType param);
    PsgParamType getCurrentParam() const { return currentParam; }
    bool hasClip() const { return currentClip.get() != nullptr; }

    std::function<void()> onClipChanged;

    // ZoomViewState::Listener overrides
    void zoomChanged() override;
    void gridChanged() override;
    void setCurrentClip(PsgClip* c);
    void changeListenerCallback(ChangeBroadcaster* cb) override;
    void selectableObjectChanged(te::Selectable* s) override;

    // CurveEditor overrides
    String getTooltip() override;
    float getValue(int idx) const;
    float getValueAt(te::EditPosition time) override;
    te::EditPosition getPointPosition(int idx) override;
    float getPointValue(int idx) override;
    float getPointCurve(int /*idx*/) override;
    void removePoint(int /*index*/) override;
    int addPoint(te::EditPosition /*time*/, float /*value*/, float /*curve*/) override;
    int getNumPoints() override;
    te::CurvePoint getBezierHandle(int /*idx*/) override;
    te::CurvePoint getBezierPoint(int /*idx*/) override;
    int nextIndexAfter(te::EditPosition time) override;
    void getBezierEnds(int /*index*/, double& x1out, float& y1out, double& x2out, float& y2out) override;
    std::pair<te::CurvePoint, te::CurvePoint> getBezierEnds(int /*index*/) override;
    int movePoint(int /*index*/, te::EditPosition /*newTime*/, float /*newValue*/, bool /*removeInterveningPoints*/) override;
    void setValueWhenNoPoints(float /*value*/) override;
    te::CurveEditorPoint* createPoint(int /*idx*/) override;
    int curvePoint(int /*index*/, float /*newCurve*/) override;
    String getCurveName() override;
    int getCurveNameOffset() override;
    te::Selectable* getItem() override;
    bool isShowingCurve() const override;
    void updateFromTrack() override;
    Colour getCurrentLineColour() override;
    Colour getCurrentFillColour() override;
    Colour getDefaultLineColour() const override;
    Colour getSelectedLineColour() const override;
    Colour getBackgroundColour() const override;
    Colour getCurveNameTextBackgroundColour() const override;
    Colour getPointOutlineColour() const override;
    Range<float> getParameterRange() const override;
    double timeScale() const;
    void paint(Graphics& g) override;
    bool hitTest(int x, int y) override;

protected:
    void paintGrid(Graphics& g);
    void showBubbleForPointUnderMouse() override;
    void hideBubble() override;

private:
    EditViewState& editViewState;
    TimelineGrid& grid;
    tracktion::SafeSelectable<PsgClip> currentClip;
    PsgParamType currentParam;
    std::optional<PsgParamList> paramList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgParamEditorComponent)
};


/** Wrapper component that adds parameter type selector in the left padding */
class PsgParamEditorWrapper : public Component,
                               private ListBoxModel,
                               private ValueTree::Listener {
public:
    PsgParamEditorWrapper(EditViewState& evs, TimelineGrid& g);
    ~PsgParamEditorWrapper() override;
    void resized() override;
    void paint(Graphics& g) override;

    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const MouseEvent&) override;

    // ValueTree::Listener overrides
    void valueTreePropertyChanged(ValueTree& tree, const Identifier& property) override;

private:
    EditViewState& editViewState_;
    PsgParamEditorComponent editor_;
    ListBox paramList_;
    TabbedComponent* tabbedComponent_ = nullptr;

    std::vector<PsgParamType::Enum> paramTypes_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgParamEditorWrapper)
};


}  // namespace MoTool