#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>


using namespace juce;

namespace MoTool {

class TimelinePanel: public Component,
                     private ChangeListener {
public:

    explicit TimelinePanel(te::Edit& edit)
        : edit_ {edit}
        , transport_ {edit_.getTransport()}
    {
        transport_.addChangeListener(this);
    }

    void resized() override {
        // TODO use layout system
    }

    void paint(Graphics& g) override {
        g.fillAll(Colours::black);
    }

    void changeListenerCallback (ChangeBroadcaster*) override {
    }

private:
    te::Edit& edit_;
    te::TransportControl& transport_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelinePanel)
};

}  // namespace MoTool
