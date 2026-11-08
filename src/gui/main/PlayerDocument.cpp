#include "PlayerDocument.h"

#include "../../controllers/App.h"
#include "../../controllers/PlayerController.h"
#include "../../util/Helpers.h"

#include <algorithm>

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
    return std::ranges::any_of(files, [](const String& path) {
        return Helpers::isTuneFile(File(path)) || File(path).hasFileExtension("uzx");
    });
}

void PlayerDocumentComponent::filesDropped(const StringArray& paths, int /*x*/, int /*y*/) {
    Array<File> files;
    for (auto& path : paths)
        files.add(File(path));
    MoToolApp::getPlayerController().handleOpenFiles(files);
}

}  // namespace MoTool
