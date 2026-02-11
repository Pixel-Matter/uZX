#pragma once

#include <JuceHeader.h>

#include "../timeline/EditComponent.h"
#include "../common/Transport.h"
#include "../devices/AYPluginSidePanel.h"

namespace MoTool {

//==============================================================================

class PlayerDocumentComponent : public Component,
                                public FileDragAndDropTarget {
public:
    PlayerDocumentComponent(te::Edit& edit, EditViewState& evs);
    ~PlayerDocumentComponent() override;

    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const StringArray& files) override;
    void filesDropped(const StringArray& files, int x, int y) override;

private:
    TransportBar transportBar_;
    EditComponent editComponent_;
    AYPluginSidePanel ayPanel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayerDocumentComponent)
};

}  // namespace MoTool
