#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../common/EditState.h"
#include "PsgClipComponent.h"


namespace MoTool {

class PsgParamEditorComponent: public te::CurveEditor {
public:
    PsgParamEditorComponent(EditViewState& evs)
        : CurveEditor(evs.edit, evs.selectionManager)
    {
    }

    String getTooltip() override {
        return "PSG Parameters editor";
    }

    float getValueAt(te::TimePosition) override {
        return 0.0f;
    }

    te::TimePosition getPointTime(int idx) override {
        return {};
    }

    float getPointValue(int idx) override {
        return 0;
    }

    float getPointCurve(int idx) override {
        return 0;
    }

    void removePoint(int index) override {

    }

    int addPoint(te::TimePosition time, float value, float curve) override {
        return 0;
    }

    int getNumPoints() override {
        return 0;
    }

    te::CurvePoint getBezierHandle(int idx) override {
        return {};
    }

    te::CurvePoint getBezierPoint(int idx) override {
        return {};
    }

    int nextIndexAfter(te::TimePosition) override {
        return 0;
    }

    void getBezierEnds(int index, double& x1out, float& y1out, double& x2out, float& y2out) override {
        x1out = 0;
        y1out = 0;
        x2out = 0;
        y2out = 0;
    }

    int movePoint(int index, te::TimePosition newTime, float newValue, bool removeInterveningPoints) override {
        return 0;
    }

    void setValueWhenNoPoints(float value) override {
        // If no points add one anyway
    }

    te::CurveEditorPoint* createPoint(int idx) override {
        return nullptr;
    }

    int curvePoint(int index, float newCurve) override {
        return 0;
    }

    juce::String getCurveName() override {
        if (currentClip != nullptr) {
            return std::string(currentParam.getLabel());
        }
        return {};
    }

    int getCurveNameOffset() override {
        return 16;
    }

    te::Selectable* getItem() override {
        if (currentClip != nullptr) {
            return currentClip;
        }
        return nullptr;
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
        return Colours::lightgrey;
    }

    juce::Colour getSelectedLineColour() const override {
        return Colours::yellow;
    }

    juce::Colour getBackgroundColour() const override {
        return getLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    }

    juce::Colour getCurveNameTextBackgroundColour() const override {
        return Colours::white.withAlpha(0.7f);
    }

    juce::Colour getPointOutlineColour() const override {
        return Colours::black;
    }


protected:
    void showBubbleForPointUnderMouse() override {
    }

    void hideBubble() override {
    }

private:
    PsgClip* currentClip = nullptr;
    PsgParamType currentParam;
    juce::Array<PsgParamFrame*> paramFrames;
};


// class PsgParamEditorComponent: public Component,
//                                private ChangeListener
// {
// public:
//     PsgParamEditorComponent(EditViewState& evs)
//         : editViewState(evs)
//     {
//         editViewState.selectionManager.addChangeListener(this);
//     }

//     ~PsgParamEditorComponent() override {
//         editViewState.selectionManager.removeChangeListener(this);
//     }

//     void changeListenerCallback(ChangeBroadcaster* sender) override {
//         // auto objects = editViewState.selectionManager.getSelectedObjects();
//         // DBG("PsgParamEditorComponent::changeListenerCallback " << objects.size());

//         // if (curveEditor != nullptr) {
//         //     removeChildComponent(curveEditor.get());
//         //     curveEditor.reset();
//         // }

//         // if (objects.size() == 1) {
//         //     if (auto* psgClip = dynamic_cast<PsgClip*>(objects[0]); psgClip != nullptr) {
//         //         curveEditor = std::make_unique<te::CurveEditor>(editViewState);
//         //         addAndMakeVisible(*curveEditor);
//         //     }
//         // }
//     }

//     void paint(Graphics& g) override {
//         g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
//     }

//     void resized() override {
//     }

// private:
//     EditViewState& editViewState;
//     std::unique_ptr<te::CurveEditor> curveEditor;
// };

}  // namespace MoTool