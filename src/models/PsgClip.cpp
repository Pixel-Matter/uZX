#include "PsgClip.h"
#include "Ids.h"
#include "PsgCC.h"
#include "PsgList.h"
#include "PsgParameter.h"
#include "EditUtilities.h"
#include "../formats/psg/PsgFile.h"
#include "../util/Oklch.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>

namespace te = tracktion;

namespace MoTool {

Colour PsgClip::colourForTrackIndex(int trackIndex) {
    // Same OKLCH lightness and chroma for every track, so only the hue differs
    // and no track's clips look brighter or duller than another's. C 0.12 is
    // the largest chroma sRGB can show at L 0.72 for every hue.
    constexpr int steps = 18;
    const float hue = 140.0f + (float) (trackIndex % steps) * 360.0f / steps;
    return Oklch::toColour({0.72f, 0.12f, std::fmod(hue, 360.0f)});
}

void PsgClip::initialise() {
    te::MidiClip::initialise();

    // load PSG list
    auto um = getUndoManager();
    auto psg = state.getChildWithName(IDs::PSG);
    if (! psg.isValid()) {
        psg = PsgList::createPsgList();
        state.addChild(psg, -1, um);
    }
    psgList = std::make_unique<PsgList>(psg, um);
    // A new list restarts its data version, so caches keyed on it must start over.
    pitchRangeVersion_ = -1;
    histogram_.reset();
    detection_.reset();
    tuning_.reset();

    // Legacy files may store per-frame beats. Back-fill machine-frame indices
    // from the edit's frame rate and drop the legacy beat property. This also
    // guarantees framesPerBeat > 0, which frame timing relies on.
    psgList->migrateToFrameIndicesIfNeeded(*this, Helpers::getProjectFps(edit), um);

    if (getColour() == getDefaultColour()) {
        // TODO make clip colors use themed palette
        setColour(colourForTrackIndex(getTrack()->getIndexInEditTrackList()));
    }
    // Not sure we should have clip plugins yet
    // ensureHasAYPlugin();
}

bool PsgClip::isInPsgList(const juce::ValueTree& v) const {
    const auto psg = state.getChildWithName(IDs::PSG);
    return psg.isValid() && (v == psg || v.isAChildOf(psg));
}

void PsgClip::valueTreePropertyChanged(juce::ValueTree& v, const juce::Identifier& id) {
    te::MidiClip::valueTreePropertyChanged(v, id);
    if ((v == state && (id == te::IDs::start || id == te::IDs::length || id == te::IDs::offset)) || isInPsgList(v))
        edit.restartPlayback();
}

void PsgClip::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) {
    te::MidiClip::valueTreeChildAdded(parent, child);
    if (parent == state && child.hasType(IDs::TIMERS))
        timersChanged();
    if (child.hasType(IDs::PSG) || child.hasType(IDs::TIMERS) || isInPsgList(parent))
        edit.restartPlayback();
}

void PsgClip::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int index) {
    te::MidiClip::valueTreeChildRemoved(parent, child, index);
    if (parent == state && child.hasType(IDs::TIMERS))
        timersChanged();
    if (child.hasType(IDs::PSG) || child.hasType(IDs::TIMERS) || isInPsgList(parent))
        edit.restartPlayback();
}

void PsgClip::timersChanged() {
    // The timers' notes and the volumes they hold are part of the range and the histogram
    pitchRangeVersion_ = -1;
    histogram_.reset();
    changed();
}

juce::MidiMessageSequence PsgClip::createPlaybackMidiSequence(te::MidiList::TimeBase timeBase) {
    auto sequence = getPsg().exportToPlaybackMidiSequence(*this, timeBase);
    timers_.addToPlaybackSequence(sequence, *this, timeBase);
    return sequence;
}

String PsgClip::getSelectableDescription() {
    return "PSG AY data clip - " + getName();
}

juce::Range<float> PsgClip::getPitchRange() const {
    auto version = getPsg().getDataVersion();
    if (pitchRangeVersion_ == version)
        return cachedPitchRange_;

    pitchRangeVersion_ = version;

    // Fold min/max over the same notes the clip painter draws (visitFrameNotes
    // is the single source of the audibility and pitch-mapping rules).
    bool hasNotes = false;
    float min = 1.0f;
    float max = 0.0f;
    const auto take = [&] (float pitch) {
        hasNotes = true;
        min = jmin(min, pitch);
        max = jmax(max, pitch);
    };

    forEachSoundingState(TimerSpan::toTheEnd, [&] (int, int, const PsgParamFrameData& data) {
        visitFrameNotes(data, [&] (const PsgFrameNote& note) { take(note.pitch); });
    });

    // A timer is drawn as a note at the pitch of what it writes
    for (int timer = 0; timer < timers_.getNumTimers(); ++timer)
        for (const auto& span : timers_.getSpans(timer))
            if (const auto pitch = span.getPitch())
                take(*pitch);

    if (! hasNotes) {
        min = 0.0f;
        max = 1.0f;
    } else {
        const float oneSemitone = 1.f / PsgParamType{PsgParamType::TonePeriodA}.getScale().octaves() / 12.f;
        const float minRange = 12.f * oneSemitone;
        if (max - min < minRange) {
            float center = (min + max) / 2.f;
            min = center - minRange * 0.5f;
            max = center + minRange * 0.5f;
        }
        // The painter centres a note on its pitch, so a note at the range
        // boundary would straddle the clip edge. Keep the padding even past
        // [0, 1] — the range is only a mapping domain, not a raw-value range.
        const float padding = oneSemitone * 1.5f;
        min -= padding;
        max += padding;
    }

    cachedPitchRange_ = { min, max };
    return cachedPitchRange_;
}

//==============================================================================
/** The machine frame the clip's source ends at: where the last stored state stops sounding. */
int PsgClip::getHistogramEndIndex() const {
    double fps = getPsg().getEffectiveFps(*this);
    if (fps <= 0.0)
        fps = getPsg().getFrameRate();
    const auto position = getPosition();
    return roundToInt((position.getOffset() + position.getLength()).inSeconds() * fps);
}

PitchHistogram PsgClip::buildPitchHistogram(int endIndex) const {
    // The list keeps only frames that change a register, so each state sounds until the
    // next stored frame: weight it by that many machine frames. The last state lasts to
    // the end of the clip's source, so a held final note counts in full too.
    PitchHistogram::Builder builder;
    forEachSoundingState(endIndex, [&] (int start, int end, const PsgParamFrameData& data) {
        builder.addFrame(data, jmax(1, end - start));
    });
    return builder.build();
}

const PitchHistogram& PsgClip::getPitchHistogram() const {
    const std::pair key {getPsg().getDataVersion(), getHistogramEndIndex()};
    if (! histogram_ || histogramKey_ != key) {
        histogramKey_ = key;
        histogram_ = buildPitchHistogram(key.second);
        ++histogramRevision_;  // never restarts, unlike a new list's data version
    }
    return *histogram_;
}

juce::Array<PsgClip*> PsgClip::getTuneClips() const {
    auto* self = const_cast<PsgClip*>(this);
    const auto& tune = state[IDs::psgTune];
    if (tune.toString().isEmpty())
        return {self};
    // ponytail: a copy of a clip keeps the id and joins the tune; unlink copies if that misleads.
    juce::Array<PsgClip*> clips;
    for (auto* track : te::getClipTracks(edit))
        for (auto* clip : track->getClips())
            if (auto* psg = dynamic_cast<PsgClip*>(clip); psg != nullptr && psg->state[IDs::psgTune] == tune)
                clips.add(psg);
    clips.addIfNotAlreadyThere(self);  // not on a track yet, or on its way out
    return clips;
}

void PsgClip::linkAsOneTune(const juce::Array<PsgClip*>& clips) {
    const auto tune = juce::Uuid().toString();
    for (auto* clip : clips)
        clip->state.setProperty(IDs::psgTune, tune, clip->getUndoManager());
}

const TuningDetection& PsgClip::getTuningDetection() const {
    const auto clips = getTuneClips();
    std::vector<std::pair<const PsgClip*, int>> key;
    for (auto* clip : clips) {
        clip->getPitchHistogram();
        key.emplace_back(clip, clip->histogramRevision_);
    }
    if (! detection_ || detectionKey_ != key) {
        detectionKey_ = std::move(key);
        detection_ = detectTuningTogether(clips);
        ++detectionRevision_;
    }
    return *detection_;
}

namespace {
const juce::Identifier* const tuningChoiceIds[] {
    &IDs::clipTuningTable, &IDs::clipTuningA4,    &IDs::clipTuningClock, &IDs::clipTuningSystem,
    &IDs::clipTuningTonic, &IDs::clipKeyTonic,    &IDs::clipKeyMinor
};

// A chosen fit has an A4 within a semitone or so of 440 Hz at a chip's clock; anything far
// outside that is a damaged or foreign value.
constexpr double minChosenA4 = 300.0, maxChosenA4 = 600.0;
constexpr double minChosenClock = 0.5e6, maxChosenClock = 4.0e6;

/** The fit stored in a clip's state, if one is there and readable. A choice made before
    clocks, systems and tonics were stored is a 12TET at the ZX clock. */
std::optional<TuningFit> readChosenFit(const juce::ValueTree& state) {
    TuningFit fit;
    fit.a4Frequency = state.getProperty(IDs::clipTuningA4, 0.0);
    fit.chipClock = state.getProperty(IDs::clipTuningClock, tuningGridClock);
    if (state.hasProperty(IDs::clipTuningSystem))
        fit.system = juce::VariantConverter<TuningSystemType>::fromVar(state[IDs::clipTuningSystem]);
    const int tonic = state.getProperty(IDs::clipTuningTonic, 0);
    const bool fitted = std::find(std::begin(fittedSystems), std::end(fittedSystems), fit.system.asEnum())
                        != std::end(fittedSystems);
    if (! (std::isfinite(fit.a4Frequency) && fit.a4Frequency >= minChosenA4 && fit.a4Frequency <= maxChosenA4)
        || ! (std::isfinite(fit.chipClock) && fit.chipClock >= minChosenClock && fit.chipClock <= maxChosenClock)
        || ! fitted || tonic < 0 || tonic >= 12)
        return std::nullopt;
    fit.tonic = Scale::Tonic(tonic);
    return fit;
}
}  // namespace

const PsgClip::Tuning& PsgClip::getTuning() const {
    getTuningDetection();  // brings detectionRevision_ up to date
    decltype(tuningChoice_) choice;
    static_assert(std::tuple_size_v<decltype(tuningChoice_)> == std::size(tuningChoiceIds));
    for (size_t i = 0; i < choice.size(); ++i)
        choice[i] = state[*tuningChoiceIds[i]];
    if (! tuning_ || tuningDetectionRevision_ != detectionRevision_ || tuningChoice_ != choice) {
        tuningDetectionRevision_ = detectionRevision_;
        tuningChoice_ = choice;
        tuning_ = resolveTuning();
        tuning_->revision = ++tuningRevision_;
    }
    return *tuning_;
}

PsgClip::Tuning PsgClip::resolveTuning() const {
    const auto& detection = getTuningDetection();
    Tuning tuning;

    const auto table = juce::VariantConverter<BuiltinTuningType>::fromVar(state[IDs::clipTuningTable]);
    if (const auto chosen = readChosenFit(state)) {
        tuning.fit = *chosen;
        tuning.grid = makeFitGrid(*chosen);
        tuning.tableLabel = getFitLabel(*chosen);
        tuning.tableChosen = true;
    } else if (state.hasProperty(IDs::clipTuningTable) && static_cast<size_t>(table) < BuiltinTuningType::size()) {
        tuning.grid = getBuiltinPeriodGrid(table);
        tuning.tableLabel = String(table.getShortLabel().data());
        tuning.table = table;
        tuning.tableChosen = true;
    } else if (detection.hasNotes) {
        tuning.grid = detection.grid;
        tuning.tableLabel = detection.getTableLabel();
        if (detection.usesFit)
            tuning.fit = detection.getFit();
        else
            tuning.table = detection.tables.front().table;
    }

    // A chosen table or clock may name every note differently (PT #0 is PT #3 a semitone
    // away, a 12TET at the Atari ST clock two semitones from the ZX one), so keys are
    // ranked again on the grid in effect.
    tuning.keys = tuning.tableChosen ? rankKeys(makePitchClassHistogram(detection.histogram, tuning.grid))
                                     : detection.keys;
    tuning.keyTie = detection.hasNotes && isKeyTie(tuning.keys);

    const int tonic = state.getProperty(IDs::clipKeyTonic, -1);
    if (tonic >= 0 && tonic < 12) {
        tuning.key = { Scale::Tonic(tonic), static_cast<bool>(state[IDs::clipKeyMinor]), 1.0f };
        tuning.hasKey = tuning.keyChosen = true;
    } else if (detection.hasNotes) {
        tuning.key = tuning.keys.front();
        tuning.hasKey = true;
    }

    tuning.uncertain = detection.hasNotes
        && ((detection.tableTie && ! tuning.tableChosen) || (tuning.keyTie && ! tuning.keyChosen));
    return tuning;
}

void PsgClip::chooseTuningTable(BuiltinTuningType table) {
    for (auto* clip : getTuneClips()) {
        auto* um = clip->getUndoManager();
        for (const auto* id : {&IDs::clipTuningA4, &IDs::clipTuningClock, &IDs::clipTuningSystem, &IDs::clipTuningTonic})
            clip->state.removeProperty(*id, um);
        clip->state.setProperty(IDs::clipTuningTable, juce::VariantConverter<BuiltinTuningType>::toVar(table), um);
    }
}

void PsgClip::chooseFit(const TuningFit& fit) {
    for (auto* clip : getTuneClips()) {
        auto* um = clip->getUndoManager();
        auto& s = clip->state;
        s.removeProperty(IDs::clipTuningTable, um);
        s.setProperty(IDs::clipTuningSystem, juce::VariantConverter<TuningSystemType>::toVar(fit.system), um);
        s.setProperty(IDs::clipTuningTonic, static_cast<int>(fit.tonic.asUnderlying()), um);
        s.setProperty(IDs::clipTuningClock, fit.chipClock, um);
        s.setProperty(IDs::clipTuningA4, fit.a4Frequency, um);
    }
}

void PsgClip::chooseKey(const DetectedKey& key) {
    for (auto* clip : getTuneClips()) {
        auto* um = clip->getUndoManager();
        clip->state.setProperty(IDs::clipKeyTonic, static_cast<int>(key.tonic), um);
        clip->state.setProperty(IDs::clipKeyMinor, key.minor, um);
    }
}

void PsgClip::clearTuningChoice() {
    for (auto* clip : getTuneClips()) {
        auto* um = clip->getUndoManager();
        for (const auto* id : tuningChoiceIds)
            clip->state.removeProperty(*id, um);
    }
}

PsgClip::Ptr PsgClip::insertTo(
    te::ClipOwner& owner,
    uZX::PsgFile& psgFile,
    te::ClipPosition position
) {
    psgFile.ensureRead();
    return insertTo(
        owner,
        psgFile.getData(),
        position,
        psgFile.getFile().getFileNameWithoutExtension()
    );
}

PsgClip::Ptr PsgClip::insertTo(
    te::ClipOwner& owner,
    uZX::PsgData& data,
    te::ClipPosition position,
    String name
) {
    auto* clip = dynamic_cast<PsgClip*>(CustomClip::insertClipWithState(
        owner,
        /*stateToUse=*/ {},
        name,
        CustomClip::Type::psg,
        position,
        te::DeleteExistingClips::yes,
        false
    ));
    // Null when the owner takes no more clips
    if (clip != nullptr)
        clip->loadFrom(data);  // same undo transaction as the insert: one undo removes the whole import
    return clip;
}

void PsgClip::loadFrom(uZX::PsgData &data) {
    auto *um = getUndoManager();

    // Fastest midi inport
    // 1. construct MidiList state detached from everything,
    // 2. remove old sequence from the state
    // 3. add the new sequence tree directly to the clips state in one operation
    getSequence().clear(um);
    double timeElapsed;
    {
        juce::ScopedTimeMeasurement measurement(timeElapsed);
        // Build the list detached and without undo; attaching it below is the
        // single undoable step.
        PsgList psg;
        psg.loadFrom(data, edit, getPosition().getStart(), nullptr);
        state.removeChild(state.getChildWithName(IDs::PSG), um);
        state.addChild(psg.state, -1, um);
        initialise();
    }
    DBG("PSG clip constructed in " << timeElapsed << "s");
    changed();
    scaleVerticallyToFit();
}

TuningDetection detectTuningTogether(const juce::Array<PsgClip*>& clips) {
    PitchHistogram together;
    for (auto* clip : clips)
        together += clip->getPitchHistogram();  // each clip's is cached
    return detectTuning(together);
}

}  // namespace MoTool
