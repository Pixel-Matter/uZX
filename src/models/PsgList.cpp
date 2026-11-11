#include "PsgList.h"
#include "PsgClip.h"
#include "PsgMidi.h"
#include "Ids.h"
#include "EditUtilities.h"

#include <cmath>
#include <limits>

namespace te = tracktion;

namespace MoTool {

static double canonicalizeFramesPerBeat(double value) {
    if (! std::isfinite(value) || value <= 0.0)
        return 0.0;

    const auto rounded = std::round(value);
    const auto tolerance = std::max(1.0e-9, std::abs(value) * 1.0e-9);
    if (std::abs(value - rounded) <= tolerance)
        return rounded;

    return value;
}

static double getBeatLengthSecondsAt(te::Edit& edit, te::TimePosition pos) {
    return edit.tempoSequence.getTempoAt(pos).getApproxBeatLength().inSeconds();
}

namespace {
    /** The persisted property of each PSG parameter, indexed by PsgParamType. */
    const juce::Identifier& paramPropertyId(size_t type) {
        static const juce::Identifier ids[] {
            IDs::va, IDs::vb, IDs::vc,  // Volume
            IDs::pa, IDs::pb, IDs::pc,  // TonePeriod
            IDs::ta, IDs::tb, IDs::tc,  // ToneIsOn
            IDs::na, IDs::nb, IDs::nc,  // NoiseIsOn
            IDs::ea, IDs::eb, IDs::ec,  // EnvelopeIsOn
            IDs::n,  IDs::e,  IDs::s,   // NoisePeriod, EnvelopePeriod, EnvelopeShape
            IDs::ra, IDs::rb, IDs::rc,  // RetriggerTone
            IDs::re,                    // RetriggerEnvelope
        };
        static_assert(std::extent_v<decltype(ids)> == PsgParamType::size());
        return ids[type];
    }

    void convertPsgFrameFromStrings(juce::ValueTree& frames) {
        if (frames.hasType(IDs::FRAME)) {
            te::convertPropertyToType<double>(frames, te::IDs::b);
            te::convertPropertyToType<int>   (frames, IDs::i); // machine-frame index
            // te::convertPropertyToType<int>   (frames, te::IDs::metadata);

            for (size_t t = 0; t < PsgParamType::size(); ++t)
                te::convertPropertyToType<int>(frames, paramPropertyId(t));
        }
        for (auto v : frames)
            convertPsgFrameFromStrings(v);
    }
}

template <typename Type>
static void removePsgParamFromSelection(Type* /*event*/) {
    // TODO
    // for (te::SelectionManager::Iterator sm; sm.next();)
    //     if (auto sme = sm->getFirstItemOfType<te::SelectedPsgParamsEvents>())
    //         sme->removeSelectedEvent(event);
}

//==============================================================================
PsgParamFrame::PsgParamFrame(const juce::ValueTree& v)
    : state {v}
    , data {}
{
    updatePropertiesFromState();
}

juce::ValueTree PsgParamFrame::createPsgFrameValueTree(int frameIndex, const PsgParamFrameData& data) {
    auto v = te::createValueTree(IDs::FRAME,
        IDs::i, frameIndex
    );
    for (size_t t = 0; t < PsgParamType::size(); ++t)
        if (const auto value = data[PsgParamType(static_cast<int>(t))])
            v.setProperty(paramPropertyId(t), *value, nullptr);
    return v;
}

te::BeatPosition PsgParamFrame::getFrameBeatPosition(const PsgClip& c) const {
    if (! hasFrameIndex())
        return {};

    // PsgClip::initialise guarantees framesPerBeat > 0 for any clip-attached list.
    const auto framesPerBeat = c.getPsg().getFramesPerBeat();
    jassert(framesPerBeat > 0.0);
    if (framesPerBeat <= 0.0)
        return {};

    return te::BeatPosition::fromBeats((double) frameIndex / framesPerBeat);
}

te::BeatPosition PsgParamFrame::getRawEditBeats(const PsgClip& c) const {
    return getFrameBeatPosition(c) - toDuration(c.getLoopStartBeats()) + toDuration(c.getContentStartBeat());
}

te::BeatPosition PsgParamFrame::getEditBeats(const PsgClip& c) const {
    return c.getQuantisation().roundBeatToNearest(getRawEditBeats(c));
}

te::TimePosition PsgParamFrame::getRawEditTime(const PsgClip& c) const {
    return c.edit.tempoSequence.toTime(getRawEditBeats(c));
}

te::TimePosition PsgParamFrame::getEditTime(const PsgClip& c) const {
    return c.edit.tempoSequence.toTime(getEditBeats(c));
}

void PsgParamFrame::setFrameIndex(int newFrameIndex, juce::UndoManager* um) {
    // Negative values would collide with the -1 "needs migration" sentinel.
    newFrameIndex = jmax(0, newFrameIndex);

    if (frameIndex != newFrameIndex) {
        state.setProperty(IDs::i, newFrameIndex, um);
        frameIndex = newFrameIndex;
    }
}

void PsgParamFrame::migrateFrameIndexFromBeat(const PsgClip& c, double frameRate, juce::UndoManager* um) {
    if (hasFrameIndex() || frameRate <= 0.0)
        return;

    if (! state.hasProperty(te::IDs::b))
        return;

    // Inverse of the load mapping: stored beat -> seconds -> nearest machine frame.
    const auto beat = te::BeatPosition::fromBeats(static_cast<double>(state.getProperty(te::IDs::b)));
    const auto timeSec = c.edit.tempoSequence.toTime(beat).inSeconds();
    setFrameIndex(roundToInt(timeSec * frameRate), um);
}

void PsgParamFrame::removeLegacyBeatProperty(juce::UndoManager* um) {
    if (state.hasProperty(te::IDs::b))
        state.removeProperty(te::IDs::b, um);
}

void PsgParamFrame::updatePropertiesFromState() noexcept {
    frameIndex  = static_cast<int>(state.getProperty(IDs::i, -1));
    data = {};  // the owning list refills the accumulated values
    for (size_t t = 0; t < PsgParamType::size(); ++t)
        if (const auto* value = state.getPropertyPointer(paramPropertyId(t)))
            data.set(PsgParamType(static_cast<int>(t)), static_cast<uint16>(static_cast<int>(*value)));
}

template<>
struct PsgList::EventDelegate<PsgParamFrame> {
    static bool isSuitableType(const juce::ValueTree& v) {
        return v.hasType(IDs::FRAME);
    }

    static bool updateObject(PsgParamFrame& e, const juce::Identifier& i) {
        e.updatePropertiesFromState();
        return i == IDs::i;
    }

    static void removeFromSelection(PsgParamFrame* e) {
        removePsgParamFromSelection(e);
    }
};

//==============================================================================
template<typename EventType>
const juce::Array<EventType*>& getEventsChecked(const juce::Array<EventType*>& events) {
    #if JUCE_DEBUG
        int lastFrameIndex = std::numeric_limits<int>::min();

        for (auto* e : events) {
            auto frameIndex = e->getFrameIndex();
            jassert(lastFrameIndex <= frameIndex);
            lastFrameIndex = frameIndex;
        }
    #endif

    return events;
}

//==============================================================================
juce::ValueTree PsgList::createPsgList() {
    return createValueTree(IDs::PSG,
                           te::IDs::ver, 1,
                           IDs::frameRate, 50.0,
                           IDs::framesPerBeat, 0.0,
                           te::IDs::channelNumber, te::MidiChannel(1));
}

PsgList::PsgList() : state (IDs::PSG) {
    state.setProperty(te::IDs::ver, 1, nullptr);
    state.setProperty(IDs::frameRate, 50.0, nullptr);
    state.setProperty(IDs::framesPerBeat, 0.0, nullptr);
    state.setProperty(te::IDs::channelNumber, te::MidiChannel(1), nullptr);
    initialise(nullptr);
}

PsgList::PsgList(const juce::ValueTree& v, juce::UndoManager* um)
    : state (v)
{
    jassert (state.hasType(IDs::PSG));
    state.setProperty(te::IDs::ver, 1, um);
    state.setProperty(te::IDs::channelNumber, te::MidiChannel(1), um);
    convertPsgFrameFromStrings(state);

    initialise(um);
}

PsgList::~PsgList() {
}

void PsgList::initialise(juce::UndoManager* um) {
    using namespace te;
    CRASH_TRACER

    midiChannel.referTo (state, te::IDs::channelNumber, um);
    frameRate_.referTo (state, IDs::frameRate, um, 0.0);
    framesPerBeat_.referTo(state, IDs::framesPerBeat, um, 0.0);

    framesList = std::make_unique<EventList<PsgParamFrame>>(state);
}

double PsgList::getFrameRate() const noexcept {
    return frameRate_.get();
}

void PsgList::setFrameRate(double fps, juce::UndoManager* um) {
    frameRate_.setValue(fps, um);
}

double PsgList::getFramesPerBeat() const noexcept {
    return canonicalizeFramesPerBeat(framesPerBeat_.get());
}

void PsgList::setFramesPerBeat(double framesPerBeat, juce::UndoManager* um) {
    framesPerBeat_.setValue(canonicalizeFramesPerBeat(framesPerBeat), um);
}

double PsgList::getEffectiveFps(const PsgClip& clip) const {
    const auto framesPerBeat = getFramesPerBeat();
    const auto beatLength = getBeatLengthSecondsAt(clip.edit, clip.getPosition().getStart());
    if (framesPerBeat <= 0.0 || beatLength <= 0.0)
        return 0.0;

    return framesPerBeat / beatLength;
}

bool PsgList::hasFpsMismatch(const PsgClip& clip, double editFps) const {
    const auto effective = getEffectiveFps(clip);
    if (effective <= 0.0 || editFps <= 0.0)
        return false;

    const auto tolerance = 1.0e-3 * jmax(effective, editFps);
    return std::abs(effective - editFps) > tolerance;
}

void PsgList::migrateToFrameIndicesIfNeeded(const PsgClip& clip, double frameRate, juce::UndoManager* um) {
    // Rate priority: the stored source rate, then the rate implied by existing
    // frames-per-beat metadata (interim files that predate frameRate), then the
    // edit's project fps. Migrating at a rate that doesn't match the legacy
    // frame spacing would collide adjacent frames onto one index.
    double fps = getFrameRate();
    if (fps <= 0.0)
        fps = getEffectiveFps(clip);
    if (fps <= 0.0)
        fps = frameRate;

    if (getNumFrames() == 0) {
        if (getFrameRate() <= 0.0 && fps > 0.0)
            setFrameRate(fps, um);
        if (getFramesPerBeat() <= 0.0 && fps > 0.0)
            setFramesPerBeat(fps * getBeatLengthSecondsAt(clip.edit, clip.getPosition().getStart()), um);
        return;
    }

    if (fps <= 0.0)
        return;

    for (auto* frame : getFrames())
        frame->migrateFrameIndexFromBeat(clip, fps, um);

    if (getFrameRate() <= 0.0)
        setFrameRate(fps, um);

    if (getFramesPerBeat() <= 0.0)
        setFramesPerBeat(fps * getBeatLengthSecondsAt(clip.edit, clip.getPosition().getStart()), um);

    for (auto* frame : getFrames())
        frame->removeLegacyBeatProperty(um);

   #if JUCE_DEBUG
    // A duplicate index means the migration rate didn't match the legacy spacing.
    // Fetch the list once: every getFrames() call re-validates the whole list in Debug,
    // which made this loop quadratic (ten seconds for a 15k-frame file).
    const auto& frames = getFrames();
    for (int i = 1; i < frames.size(); ++i)
        jassert(frames[i]->getFrameIndex() != frames[i - 1]->getFrameIndex());
   #endif
}

void PsgList::recomputeAccumulatedState(const juce::Array<PsgParamFrame*>& frames) const {
    ++dataVersion_;
    PsgParamFrameData accumulated;
    accumulated.resetMixer();
    for (auto* frame : frames) {
        // Merge this frame's changes into accumulated state
        for (size_t i = 0; i < PsgParamType::size(); ++i) {
            if (frame->data.isSet(PsgParamType(static_cast<int>(i)))) {
                accumulated.values[i] = frame->data.values[i];
            }
        }
        // Copy accumulated values to frame (preserving frame's masks)
        auto savedMasks = frame->data.masks;
        frame->data.values = accumulated.values;
        frame->data.masks = savedMasks;
    }
}

void PsgList::clear (juce::UndoManager* um) {
    // Timing metadata (frameRate, framesPerBeat) is kept: it describes the list's
    // relation to tempo, not its content.
    state.removeAllChildren(um);
}

void PsgList::setMidiChannel(te::MidiChannel newChannel) {
    midiChannel = newChannel;
}

PsgParamFrame* PsgList::addFrameEvent(int frameIndex, const PsgParamFrameData& data, juce::UndoManager* um) {
    auto v = PsgParamFrame::createPsgFrameValueTree(frameIndex, data);
    state.addChild(v, -1, um);
    return framesList->getEventFor(v);
}



void PsgList::loadFrom(const uZX::PsgData &data, te::Edit& edit, juce::UndoManager* um) {
    PsgParamFrameData params;
    uZX::PsgRegsFrame regsState;
    // uZX::PsgRegsFrame regsFromParamsState;
    params.resetMixer();  // because AY regs after reset has all NNNTTT flags set (bits==0)

    // Track last envelope shape and retrigger state for state transitions
    std::optional<uint8_t> lastEnvelopeShape;
    bool lastRetriggerState = false;  // Track if last frame had retrigger=1

    setFrameRate(data.getFrameRate(), um);
    setFramesPerBeat(data.getFrameRate() * getBeatLengthSecondsAt(edit, 0_tp), um);

    for (size_t i = 0; i < data.frames.size(); i++) {
        auto &frame = data.frames[i];
        if (frame.isEmpty()) {
            continue;
        }
        regsState.clear();
        regsState.update(data.frames[i]);

        params.clear();
        params.update(regsState);  // tracks really changed params

        bool currentRetriggerState = false;

        // Detect envelope shape retrigger and manage state transitions
        if (frame.hasEnvelopeShapeSet()) {
            auto currentShape = frame.getEnvelopeShape();

            if (lastEnvelopeShape.has_value() && lastEnvelopeShape.value() == currentShape) {
                // Same envelope shape = retrigger!
                params.set(PsgParamType::RetriggerEnvelope, 1);
                params.set(PsgParamType::EnvelopeShape, currentShape);
                currentRetriggerState = true;
            } else {
                // Value changed or first write - explicitly turn off retrigger if it was on
                if (lastRetriggerState) {
                    params.set(PsgParamType::RetriggerEnvelope, 0);
                }
                currentRetriggerState = false;
            }

            lastEnvelopeShape = currentShape;
        } else {
            // No envelope shape write this frame - turn off retrigger if it was on
            if (lastRetriggerState) {
                params.set(PsgParamType::RetriggerEnvelope, 0);
            }
            currentRetriggerState = false;
        }

        lastRetriggerState = currentRetriggerState;

        auto v = PsgParamFrame::createPsgFrameValueTree(static_cast<int>(i), params);
        state.addChild(v, -1, um);
    }
}

const juce::Array<PsgParamFrame*>& PsgList::getFrames() const {
    jassert (framesList != nullptr);
    const auto& frames = getEventsChecked(framesList->getSortedList());
    if (framesList->dataChanged) {
        framesList->dataChanged = false;
        recomputeAccumulatedState(frames);
    }
    return frames;
}

//==============================================================================
const PsgParamFrame* PsgList::getFrameAtIndex(int frameIndex) const {
    const auto& frames = getFrames();
    if (frames.isEmpty())
        return nullptr;

    int low = 0;
    int high = frames.size() - 1;

    while (low <= high) {
        int mid = (low + high) / 2;
        auto midFrameIndex = frames[mid]->getFrameIndex();

        if (midFrameIndex == frameIndex)
            return frames[mid];
        else if (midFrameIndex < frameIndex)
            low = mid + 1;
        else
            high = mid - 1;
    }

    return nullptr;
}

double PsgList::getTimeInBase(const PsgParamFrame& frame, PsgClip& clip, te::MidiList::TimeBase timeBase) const {
    switch (timeBase) {
        case te::MidiList::TimeBase::beatsRaw:
            return frame.getFrameBeatPosition(clip).inBeats();
        case te::MidiList::TimeBase::beats:
            return std::max(0_bp, frame.getEditBeats(clip) - toDuration(clip.getStartBeat())).inBeats();
        case te::MidiList::TimeBase::seconds:
            [[fallthrough]];
        default:
            return std::max(0_tp, frame.getEditTime(clip) - toDuration(clip.getPosition().getStart())).inSeconds();
    }
}

[[nodiscard]] juce::MidiMessageSequence PsgList::exportToPlaybackMidiSequence(PsgClip& clip, te::MidiList::TimeBase timeBase) const {
    // DBG("Exporting PSG to MIDI sequence, channel " << getMidiChannel().getChannelNumber() << ", timebase " << (timeBase == te::MidiList::TimeBase::beats ? "beats" : "seconds"));
    PsgParamsMidiWriter writer {getMidiChannel().getChannelNumber()};
    const auto& frames = getFrames();
    for (int i = 0; i < frames.size(); ++i) {
        auto* f = frames[i];
        if (i == 0) {
            // Export the complete accumulated initial state at time 0.
            // PSG frames only mask registers that changed, but recomputeAccumulatedState()
            // copies the full accumulated values (including the resetMixer baseline and
            // defaults) into every frame. Forcing all masks on at frame 0 emits a complete
            // AY register snapshot, so playback/reposition always starts from a deterministic
            // state instead of inheriting stale registers from a previous play.
            PsgParamFrameData fullState = f->getData();
            std::fill(fullState.masks.begin(), fullState.masks.end(), true);
            writer.write(getTimeInBase(*f, clip, timeBase), fullState);
        } else {
            writer.write(getTimeInBase(*f, clip, timeBase), f->getData());
        }
    }
    return writer.getSequence();
}

}  // namespace MoTool
