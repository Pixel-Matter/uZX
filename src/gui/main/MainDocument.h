#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from JUCE

#include "../timeline/EditComponent.h"
#include "../layout/Layout.h"
#include "../common/Transport.h"


using namespace juce;

namespace MoTool {

namespace lo = Layout;

//==============================================================================

class MainDocumentComponent: public Component {
public:

    explicit MainDocumentComponent(te::Edit& edit, EditViewState& evs)
        : transportBar_ {edit}
        , editComponent_ {edit, evs}
        // , footer_        {edit_.engine,}
    {
        using namespace Layout::Operators;  // for operator>>
        Helpers::addLayoutItemsAndMakeVisible(*this, layout_,
            transportBar_  >> 32_px,
            editComponent_ >> 1_fr
            // footer_        >> 32_px
        );
    }

    ~MainDocumentComponent() override {
    }

    void resized() override {
        layout_.performLayout(getLocalBounds());
    }

private:
    TransportBar transportBar_;
    EditComponent editComponent_;  // former AudioTimeline
    // FooterBar footer_;

    lo::VerticalLayout layout_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainDocumentComponent)
};

}  // namespace MoTool
