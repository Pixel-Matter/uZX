#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool {

class PsgClip : public te::MidiClip {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<MidiClip>;

};

}  // namespace MoTool