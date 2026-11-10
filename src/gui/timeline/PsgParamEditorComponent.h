#pragma once

#include <JuceHeader.h>

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


/** Shows one parameter of the last selected PSG clip as a stepped line. Display only: a click selects the clip. */
class PsgParamEditorComponent: public Component,
                               private ZoomViewState::Listener,
                               private TimelineGrid::Listener,
                               private ChangeListener
{
public:
    PsgParamEditorComponent(EditViewState& evs, TimelineGrid& g);
    ~PsgParamEditorComponent() override;

    void setCurrentParam(PsgParamType param);
    PsgParamType getCurrentParam() const { return currentParam; }
    /** False once the shown clip is deleted. */
    bool hasClip() const { return currentClip.get() != nullptr; }

    void paint(Graphics& g) override;
    bool hitTest(int x, int y) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseEnter(const MouseEvent&) override { repaint(); }
    void mouseExit(const MouseEvent&) override { repaint(); }

private:
    void viewRangeChanged() override { repaint(); }
    void gridChanged() override { repaint(); }
    void changeListenerCallback(ChangeBroadcaster* cb) override;

    float timeToX(te::TimePosition t) const;
    te::TimePosition xToTime(float x) const;
    float valueToY(float value) const;
    Point<float> getPosition(const PsgParamList& params, int idx) const;
    void paintGrid(Graphics& g);

    static constexpr float pointRadius = 5.0f;

    EditViewState& editViewState;
    TimelineGrid& grid;
    // Frames are read through the clip at each use, so a deleted clip leaves nothing dangling
    tracktion::SafeSelectable<PsgClip> currentClip;
    PsgParamType currentParam;

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