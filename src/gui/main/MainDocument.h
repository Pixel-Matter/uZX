#pragma once

#include <JuceHeader.h>

#include "../timeline/EditComponent.h"
#include "../common/Transport.h"


using namespace juce;

namespace MoTool {

//==============================================================================

class MainDocumentComponent: public Component {
public:

    explicit MainDocumentComponent(te::Edit& edit, EditViewState& evs)
        : transportBar_ {edit}
        , editComponent_ {edit, evs}
        // , footer_        {edit_.engine,}
    {
        addAndMakeVisible(transportBar_);
        addAndMakeVisible(editComponent_);
        // addAndMakeVisible(footer_);
    }

    ~MainDocumentComponent() override {}

    void resized() override {
        auto r = getLocalBounds();
        auto transportBarHeight = 32;
        transportBar_.setBounds(r.removeFromTop(transportBarHeight));
        editComponent_.setBounds(r);
    }

private:
    TransportBar transportBar_;
    EditComponent editComponent_;  // former AudioTimeline
    // FooterBar footer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainDocumentComponent)
};

}  // namespace MoTool
