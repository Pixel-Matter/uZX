#include "PlayerDocument.h"

#include "../../controllers/App.h"

namespace MoTool {

PlayerDocumentComponent::PlayerDocumentComponent(te::Edit& edit, EditViewState& evs)
    : transportBar_ {evs, {.showRecord = false, .showAutomation = false}}
    , editComponent_ {edit, evs, {.showDetailsPanel = false, .autoFitTrackHeights = true, .transparentClips = true, .disableClipSelection = true}}
    , ayPanel_ {edit, MoToolApp::getSelectionManager()}
{
    addAndMakeVisible(transportBar_);
    addAndMakeVisible(editComponent_);
    addAndMakeVisible(ayPanel_);
}

PlayerDocumentComponent::~PlayerDocumentComponent() {}

void PlayerDocumentComponent::resized() {
    auto r = getLocalBounds();
    static constexpr int transportBarHeight = 40;
    static constexpr int ayPanelWidth = 240;

    transportBar_.setBounds(r.removeFromTop(transportBarHeight));
    ayPanel_.setBounds(r.removeFromRight(ayPanelWidth));
    editComponent_.setBounds(r);
}

}  // namespace MoTool
