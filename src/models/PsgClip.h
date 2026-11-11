#pragma once

#include <JuceHeader.h>
#include <memory>

#include "CustomClip.h"
#include "PsgList.h"
#include "../formats/psg/PsgFile.h"
#include "tuning/TuningDetector.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

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

    /** Colour a new clip gets on the track at this index: hues rotate in
        OKLCH at constant lightness and chroma. */
    static Colour colourForTrackIndex(int trackIndex);

    PsgList& getPsg() const noexcept {
        jassert(psgList != nullptr);
        return *psgList;
    }

    /** Returns the normalized pitch range across all frames in the clip.
        Cached and automatically invalidated when PSG data changes. */
    juce::Range<float> getPitchRange() const;

    //==============================================================================
    /** The clips of this clip's tune, this one included: the PSG clips of its edit imported
        with it, such as the chips of a Turbo Sound tune. Just this one when it came alone. */
    juce::Array<PsgClip*> getTuneClips() const;
    /** Makes the clips one tune: tuning and key are detected, and chosen, over all of them. */
    static void linkAsOneTune(const juce::Array<PsgClip*>& clips);

    /** Tuning table and key detection over the tune's clips (see getTuneClips), run once
        per change of their histograms. */
    const TuningDetection& getTuningDetection() const;
    /** Advances whenever getTuningDetection() is recomputed. */
    int getTuningDetectionRevision() const { getTuningDetection(); return detectionRevision_; }
    /** This clip's own pitches, cached until its data or its end frame (clip offset and
        length at the effective frame rate) changes. */
    const PitchHistogram& getPitchHistogram() const;

    /** The grid and key in effect: the user's choice stored in the clip state, else the
        detection. A stored choice this build cannot read (an unknown table name, an A4 or
        tonic out of range) is ignored. A clip without audible pitches has neither table
        nor key unless the user chose one. `uncertain` is a detection tie no choice settled. */
    struct Tuning {
        PeriodGrid grid;    // empty when there is no table
        String tableLabel;  // empty when there is no table
        std::optional<BuiltinTuningType> table;  // set when a built-in table is in effect
        std::optional<TuningFit> fit;            // set when a fitted tuning is in effect
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

    /** Cached until the detection or the clip's tuning properties change. */
    const Tuning& getTuning() const;

    /** A choice applies to every clip of the tune. */
    void chooseTuningTable(BuiltinTuningType table);
    /** A fitted tuning, from TuningDetection::fits or re-expressed by atClock(). */
    void chooseFit(const TuningFit& fit);
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

protected:
    // Tracktion rebuilds playback when its own clip types move or their notes change. A PSG clip's
    // events come from its PSG list, so it asks for the rebuild when the list or its position changes.
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;

private:
    /** The PSG list's tree or one inside it: frames, frame rate, frames per beat. */
    bool isInPsgList(const juce::ValueTree&) const;

    std::unique_ptr<PsgList> psgList;
    mutable juce::Range<float> cachedPitchRange_ { 0.0f, 1.0f };
    mutable int pitchRangeVersion_ = -1;
    mutable std::optional<PitchHistogram> histogram_;
    mutable std::pair<int, int> histogramKey_ {-1, -1};  // data version, end frame
    mutable int histogramRevision_ = 0;
    mutable std::optional<TuningDetection> detection_;
    mutable std::vector<std::pair<const PsgClip*, int>> detectionKey_;  // tune clips and histogram revisions
    mutable int detectionRevision_ = 0;
    mutable std::optional<Tuning> tuning_;
    mutable int tuningDetectionRevision_ = -1;
    mutable std::array<juce::var, 7> tuningChoice_;
    mutable int tuningRevision_ = 0;

    Tuning resolveTuning() const;
    int getHistogramEndIndex() const;
    PitchHistogram buildPitchHistogram(int endIndex) const;

    void loadFrom(uZX::PsgData &data);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsgClip)
};

/** Tuning table and key detected over clips that play together, such as the chips of a
    Turbo Sound tune: each chip plays only part of the music, the key is in all of them. */
TuningDetection detectTuningTogether(const juce::Array<PsgClip*>& clips);

}  // namespace MoTool