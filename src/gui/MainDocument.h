#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>  // from JUCE

#include "timeline/MidiTimeline.h"
#include "layout/Layout.h"
#include "Transport.h"


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

    explicit MainDocumentComponent(te::Edit& edit)
        : edit_ {edit}
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
    // std::unique_ptr<EditComponent> editComponent_;

    TransportBar transportBar_ {edit_};
    // TimelinePanel timelinePanel_     {edit_};
    MidiTimeline timelinePanel_ {edit_};
    // FooterBar footer_                {edit_.engine,};

    lo::VerticalLayout layout_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainDocumentComponent)
};

}  // namespace MoTool
