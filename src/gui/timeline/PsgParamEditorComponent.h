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
    float getMaxValue() const;
    int findIndex(te::TimePosition pos) const;
    int findPrevActiveIndex(int idx) const;
    bool isActive(int idx) const;
    te::TimePosition getTime(int idx) const;
    float getValue(int idx) const;
    float getValueAt(te::TimePosition time) const;

private:
    const PsgClip& clip;
    const PsgParamType paramType;
    const juce::Array<PsgParamFrame*>& frames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgParamList)
};


class PsgParamEditorComponent: public te::CurveEditor,
                               public ZoomViewState::Listener
{
public:
    PsgParamEditorComponent(EditViewState& evs, TimelineGrid& g);
    ~PsgParamEditorComponent() override;

    // ZoomViewState::Listener overrides
    void zoomChanged() override;
    void setCurrentClip(PsgClip* c);
    void changeListenerCallback(juce::ChangeBroadcaster* cb) override;
    void selectableObjectChanged(te::Selectable* s) override;

    // CurveEditor overrides
    String getTooltip() override;
    float getValue(int idx) const;
    float getValueAt(te::TimePosition time) override;
    te::TimePosition getPointTime(int idx) override;
    float getPointValue(int idx) override;
    float getPointCurve(int /*idx*/) override;
    void removePoint(int /*index*/) override;
    int addPoint(te::TimePosition /*time*/, float /*value*/, float /*curve*/) override;
    int getNumPoints() override;
    te::CurvePoint getBezierHandle(int /*idx*/) override;
    te::CurvePoint getBezierPoint(int /*idx*/) override;
    int nextIndexAfter(te::TimePosition time) override;
    void getBezierEnds(int /*index*/, double& x1out, float& y1out, double& x2out, float& y2out) override;
    int movePoint(int /*index*/, te::TimePosition /*newTime*/, float /*newValue*/, bool /*removeInterveningPoints*/) override;
    void setValueWhenNoPoints(float /*value*/) override;
    te::CurveEditorPoint* createPoint(int /*idx*/) override;
    int curvePoint(int /*index*/, float /*newCurve*/) override;
    juce::String getCurveName() override;
    int getCurveNameOffset() override;
    te::Selectable* getItem() override;
    bool isShowingCurve() const override;
    void updateFromTrack() override;
    juce::Colour getCurrentLineColour() override;
    juce::Colour getCurrentFillColour() override;
    juce::Colour getDefaultLineColour() const override;
    juce::Colour getSelectedLineColour() const override;
    juce::Colour getBackgroundColour() const override;
    juce::Colour getCurveNameTextBackgroundColour() const override;
    juce::Colour getPointOutlineColour() const override;
    double timeScale() const;
    void paint(juce::Graphics& g) override;
    bool hitTest(int x, int y) override;

protected:
    void paintGrid(juce::Graphics& g);
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

}  // namespace MoTool