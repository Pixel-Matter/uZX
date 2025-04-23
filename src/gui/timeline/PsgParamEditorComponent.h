#pragma once

#include <JuceHeader.h>

#include "../common/EditState.h"

namespace MoTool {

class PsgParamEditorComponent: public Component {
public:
    PsgParamEditorComponent(EditViewState& evs)
        : editViewState(evs)
    {
    }

    ~PsgParamEditorComponent() override {}

    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
    }

private:
    EditViewState& editViewState;
};

}  // namespace MoTool