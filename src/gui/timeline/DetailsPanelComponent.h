#pragma once

#include <JuceHeader.h>

#include "../common/LookAndFeel.h"
#include "../../controllers/EditState.h"
#include "PsgParamEditorComponent.h"

namespace MoTool {

class DetailsPanelComponent: public Component {
public:
    DetailsPanelComponent(EditViewState& evs)
        : editViewState(evs)
        , psgEditor(evs)
    {
        addAndMakeVisible(psgEditor);
    }

    ~DetailsPanelComponent() override {}

    void paint(Graphics& g) override {
        g.fillAll(Colors::Theme::backgroundAlt);
    }

    void resized() override {
        auto bounds = getLocalBounds();
        // reduce fro left and right
        bounds.removeFromLeft(editViewState.showHeaders ? editViewState.headersWidth : 0);
        bounds.removeFromRight(editViewState.showFooters ? 100 : 0);
        // reduce from top and bottom
        psgEditor.setBounds(bounds.reduced(0, 8));
    }

private:
    [[maybe_unused]] EditViewState& editViewState;
    PsgParamEditorComponent psgEditor;
};

}  // namespace MoTool