#pragma once

#include <JuceHeader.h>
#include <common/Utilities.h>


using namespace juce;

namespace MoTool {

//==============================================================================
/** TimelinePanel
    Ideal modular interface:
    Timeline consist of tracks one under the other
    Tracks can be sorted and moved around, added and deleted
    Dragging from track to empty space creates a new track
    Track can be one of:
      - Marker: for labeling sections of the timeline
      - Audio/Video: for prototyping
      - MIDI: for prototyping or for downmixing to PSG tracks
      - MIDI FX: for processing MIDI events
      - PSG: music data for playback on a machine
      - FX: Python, Lua, JS, C++, Asm plugins, for video effects prototyping
      - Machine: emulated machine with code and data

    == TODO ==
    - Implement track adding and deleting
*/

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
