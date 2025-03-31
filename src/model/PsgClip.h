#pragma once

#include <JuceHeader.h>
#include <memory>

#include "CustomClip.h"
#include "PsgList.h"
#include "../formats/psg/PsgFile.h"

namespace te = tracktion;

namespace MoTool {

class PsgClip : public te::MidiClip, public CustomClip {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<PsgClip>;

    PsgClip(const juce::ValueTree& v, te::EditItemID id, te::ClipOwner& parent_)
        : te::MidiClip(v, id, parent_)
    {}

    void initialise() override;

    String getSelectableDescription() override;

    Colour getDefaultColour() const override {
        return Colours::blue;
    }

    static Ptr insertTo(
        te::ClipOwner& owner,
        uZX::PsgFile& psgFile,
        te::ClipPosition position
    );

private:
    std::unique_ptr<PsgList> psgList;

    void loadFromFile(uZX::PsgFile &psgFile);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

}  // namespace MoTool