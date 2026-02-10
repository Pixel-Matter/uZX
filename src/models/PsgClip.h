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

    PsgList& getPsg() const noexcept {
        jassert(psgList != nullptr);
        return *psgList;
    }

    /** Returns the normalized pitch range across all frames in the clip.
        Cached and automatically invalidated when PSG data changes. */
    juce::Range<float> getPitchRange() const;

    static Ptr insertTo(
        te::ClipOwner& owner,
        uZX::PsgFile& psgFile,
        te::ClipPosition position
    );

    static Ptr insertTo(
        te::ClipOwner& owner,
        uZX::PsgData& data,
        te::ClipPosition position,
        String name = {}
    );

private:
    std::unique_ptr<PsgList> psgList;
    mutable juce::Range<float> cachedPitchRange_ { 0.0f, 1.0f };
    mutable int pitchRangeVersion_ = -1;

    void loadFrom(uZX::PsgData &data);
    void loadFrom(uZX::PsgFile &psgFile);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

}  // namespace MoTool