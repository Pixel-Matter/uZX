#pragma once

#include <JuceHeader.h>
#include <memory>

#include "CustomClip.h"
#include "PsgList.h"
#include "../formats/psg/PsgFile.h"
#include "tuning/TuningDetector.h"

#include <array>
#include <optional>

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

    //==============================================================================
    /** Tuning table and key detection over this clip's data, run once per data version. */
    const TuningDetection& getTuningDetection() const;
    PitchHistogram buildPitchHistogram() const;

    /** The grid and key in effect: the user's choice stored in the clip state, else the
        detection. A stored choice this build cannot read (an unknown table name, an A4 or
        tonic out of range) is ignored. A clip without audible pitches has neither table
        nor key unless the user chose one. `uncertain` is a detection tie no choice settled. */
    struct Tuning {
        PeriodGrid grid;    // empty when there is no table
        String tableLabel;  // empty when there is no table
        std::optional<BuiltinTuningType> table;  // set when a built-in table is in effect
        bool isEqualTemperament = false;         // set when a 12TET grid is in effect
        DetectedKey key;    // meaningful only when hasKey
        /** The 24 keys ranked on this grid: a chosen table names the notes differently
            from the detected one, so the ranking follows the grid in effect. */
        std::vector<DetectedKey> keys;
        bool keyTie = false;  // on this grid; meaningless without notes
        bool hasKey = false;
        bool tableChosen = false;
        bool keyChosen = false;
        bool uncertain = false;
        int revision = 0;   // changes whenever any field above may have changed
    };

    /** Cached until the clip's data or its tuning properties change. */
    const Tuning& getTuning() const;

    void chooseTuningTable(BuiltinTuningType table);
    void chooseEqualTemperament(double a4Frequency);
    void chooseKey(const DetectedKey& key);
    void clearTuningChoice();

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
    mutable std::optional<TuningDetection> detection_;
    mutable int detectionVersion_ = -1;
    mutable std::optional<Tuning> tuning_;
    mutable int tuningVersion_ = -1;
    mutable std::array<juce::var, 4> tuningChoice_;
    mutable int tuningRevision_ = 0;

    Tuning resolveTuning() const;

    void loadFrom(uZX::PsgData &data);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

}  // namespace MoTool