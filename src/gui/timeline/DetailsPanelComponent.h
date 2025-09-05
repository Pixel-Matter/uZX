#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"
#include "PsgParamEditorComponent.h"

namespace MoTool {

class DetailsPanelComponent: public Component {
public:
    DetailsPanelComponent(EditViewState& evs, TimelineGrid& g);
    ~DetailsPanelComponent() override;

    void paint(Graphics& g) override;
    void resized() override;

private:
    EditViewState& editViewState;
    PsgParamEditorComponent psgEditor;
};

}  // namespace MoTool