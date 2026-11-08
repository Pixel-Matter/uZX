#pragma once

#include <JuceHeader.h>

#include "../../models/PsgClip.h"

namespace MoTool {

//==============================================================================
/** Lets the user settle a PSG clip's tuning and key when detection is unsure, or override
    them. Tracker tables and fitted tunings (12TET, just intonations) are listed with their
    scores; a fitted tuning also takes a chip clock, which renames its notes and leaves its
    score alone. Shows the ranked keys with their scores and the clip's
    pitch classes at sub-semitone resolution on the selected grid, so a wrong grid is
    visible as peaks between the lines. Choices are written to the clip state at once;
    "Use detected" removes them.

    The picker holds the clip weakly and closes itself when the clip is deleted, which
    also happens when its Edit is replaced or closed: a clip must never outlive its Edit.
    Lists follow the clip's data, so they stay right while the clip is edited. */
class TuningPickerComponent : public Component,
                              private juce::ValueTree::Listener,
                              private te::SelectableListener,
                              private juce::AsyncUpdater
{
public:
    explicit TuningPickerComponent(PsgClip& clip);
    ~TuningPickerComponent() override;

    /** Opens the picker for a clip in its own window. */
    static void show(PsgClip& clip);

    void resized() override;

private:
    class HistogramView;

    te::SafeSelectable<PsgClip> clip_;
    juce::ValueTree clipState_;  // kept so the listener can be removed after the clip is gone
    int listedDataVersion_ = -1;

    Label tableLabel_{{}, "Tuning"};
    Label clockLabel_{{}, "Chip clock"};
    Label a4Label_;
    Label keyLabel_{{}, "Key"};
    ComboBox clockBox_;
    ComboBox tableBox_;
    ComboBox keyBox_;
    TextButton detectedButton_{"Use detected"};
    std::unique_ptr<HistogramView> histogramView_;

    // Items are listed in detection order; the combo IDs below index into these copies,
    // never into a detection that may have been recomputed since.
    std::vector<BuiltinTuningType> listedTables_;
    std::vector<TuningFit> listedFits_;
    std::vector<DetectedKey> listedKeys_;

    static constexpr int firstFitItemId = 1000;

    void rebuildLists(const PsgClip& clip);
    void rebuildKeyList(const std::vector<DetectedKey>& keys);
    String getListedFitText(size_t index) const;
    void refresh();
    void close();

    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override { triggerAsyncUpdate(); }
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;

    void selectableObjectChanged(te::Selectable*) override {}
    void selectableObjectAboutToBeDeleted(te::Selectable*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPickerComponent)
};

}  // namespace MoTool
