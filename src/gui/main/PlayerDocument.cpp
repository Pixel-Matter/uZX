#include "PlayerDocument.h"

#include "../../controllers/App.h"
#include "../../controllers/PlayerController.h"

namespace MoTool {

PlayerDocumentComponent::PlayerDocumentComponent(te::Edit& edit, EditViewState& evs)
    : transportBar_ {evs, {.showRecord = false, .showAutomation = false}}
    , editComponent_ {edit, evs, {.showDetailsPanel = false, .autoFitTrackHeights = true, .transparentClips = true, .disableClipSelection = true}}
{
    if (! evs.vizMode)
        addAndMakeVisible(transportBar_);
    addAndMakeVisible(editComponent_);
}

PlayerDocumentComponent::~PlayerDocumentComponent() {}

void PlayerDocumentComponent::resized() {
    auto r = getLocalBounds();
    static constexpr int transportBarHeight = 40;

    if (transportBar_.isVisible())
        transportBar_.setBounds(r.removeFromTop(transportBarHeight));
    editComponent_.setBounds(r.withTrimmedLeft(8));
}

bool PlayerDocumentComponent::isInterestedInFileDrag(const StringArray& files) {
    for (auto& path : files) {
        auto ext = File(path).getFileExtension().toLowerCase();
        if (ext == ".psg" || ext == ".uzx")
            return true;
    }
    return false;
}

void PlayerDocumentComponent::filesDropped(const StringArray& paths, int /*x*/, int /*y*/) {
    Array<File> files;
    for (auto& path : paths)
        files.add(File(path));
    MoToolApp::getPlayerController().handleOpenFiles(files);
}

}  // namespace MoTool
