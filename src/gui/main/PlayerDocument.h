#pragma once

#include <JuceHeader.h>

#include "../timeline/EditComponent.h"
#include "../common/Transport.h"

namespace MoTool {

//==============================================================================

class VizHint;

class PlayerDocumentComponent : public Component,
                                public FileDragAndDropTarget {
public:
    PlayerDocumentComponent(te::Edit& edit, EditViewState& evs);
    ~PlayerDocumentComponent() override;

    /** In Viz mode, which hides every control: tells for a moment at the top what it can show, which
        of that it shows, and the keys to switch (V, S) and to leave. Only on screen, never in a video. */
    void showVizHint();

    void paint(Graphics& g) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const StringArray& files) override;
    void filesDropped(const StringArray& files, int x, int y) override;

private:
    EditViewState& editViewState_;
    TransportBar transportBar_;
    EditComponent editComponent_;
    std::unique_ptr<VizHint> vizHint_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayerDocumentComponent)
};

}  // namespace MoTool
