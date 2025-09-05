#include "DetailsPanelComponent.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

DetailsPanelComponent::DetailsPanelComponent(EditViewState& evs, TimelineGrid& g)
    : editViewState(evs)
    , psgEditor(evs, g)
{
    addAndMakeVisible(psgEditor);
}

DetailsPanelComponent::~DetailsPanelComponent() {
}

void DetailsPanelComponent::paint(Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
}

void DetailsPanelComponent::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(editViewState.showHeaders ? editViewState.headersWidth : 0);
    bounds.removeFromRight(editViewState.showFooters ? editViewState.footerWidth : 0);
    psgEditor.setBounds(bounds.reduced(0, 8));
}

}  // namespace MoTool