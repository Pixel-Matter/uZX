#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "PsgParamEditorComponent.h"

namespace MoTool {

class DetailsPanelComponent: public Component {
public:
    DetailsPanelComponent(EditViewState& evs)
        : editViewState(evs)
        , psgEditor(evs)
    {
        // addAndMakeVisible(psgEditor);
    }

    ~DetailsPanelComponent() override {}

    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {

        auto bounds = getLocalBounds();
        psgEditor.setBounds(bounds);
    }

private:
    [[maybe_unused]] EditViewState& editViewState;
    PsgParamEditorComponent psgEditor;
};

}  // namespace MoTool