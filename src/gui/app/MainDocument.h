#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from JUCE

#include "../timeline/AudioTimeline.h"
#include "../layout/Layout.h"
#include "../common/Transport.h"


using namespace juce;

namespace MoTool {

namespace lo = Layout;

//==============================================================================
/** MainDocumentComponent
    Ideal modular interface:
    We have a timeline panel, transport bar, and footer bar
    Timeline can playback and record,
    we can hear the audio and view the display (not shown)
*/

class MainDocumentComponent: public Component {
public:

    explicit MainDocumentComponent(te::Edit& edit, EditViewState& evs, te::SelectionManager& selectionManager)
        : edit_ {edit}
        , selectionManager_ {selectionManager}
        , transportBar_ {edit_}
        , timelinePanel_ {edit_, evs, selectionManager_}
        // , footer_        {edit_.engine,}
    {
        using namespace Layout::Operators;  // for operator>>
        Helpers::addLayoutItemsAndMakeVisible(*this, layout_,
            transportBar_  >> 32_px,
            timelinePanel_ >> 1_fr
            // footer_        >> 32_px
        );
    }

    ~MainDocumentComponent() override {
    }

    void resized() override {
        layout_.performLayout(getLocalBounds());
    }

private:
    te::Edit& edit_;
    te::SelectionManager& selectionManager_;

    TransportBar transportBar_;
    AudioTimeline timelinePanel_;
    // FooterBar footer_;

    lo::VerticalLayout layout_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainDocumentComponent)
};

}  // namespace MoTool
