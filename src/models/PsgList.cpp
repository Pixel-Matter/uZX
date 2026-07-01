#include "PsgList.h"
#include "PsgClip.h"
#include "PsgMidi.h"
#include "Ids.h"
#include "EditUtilities.h"

#include <cmath>

namespace te = tracktion;

namespace MoTool {

static double roundTo(double value, int decimalPlaces = 3) {
    double factor = std::pow(10.0, decimalPlaces);
    return std::round(value * factor) / factor;
}

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

static double getFramesPerBeatFromGeometry(const juce::Array<PsgParamFrame*>& frames);

namespace {
    void convertPsgFrameFromStrings(juce::ValueTree& frames) {
        if (frames.hasType(IDs::FRAME)) {
            te::convertPropertyToType<double>(frames, te::IDs::b);
            // te::convertPropertyToType<int>   (frames, te::IDs::metadata);

            te::convertPropertyToType<int> (frames, IDs::va); // VolumeA
            te::convertPropertyToType<int> (frames, IDs::vb); // VolumeB
            te::convertPropertyToType<int> (frames, IDs::vc); // VolumeC
            te::convertPropertyToType<int> (frames, IDs::pa); // TonePitchA
            te::convertPropertyToType<int> (frames, IDs::pb); // TonePitchB
            te::convertPropertyToType<int> (frames, IDs::pc); // TonePitchC
            te::convertPropertyToType<int> (frames, IDs::ta); // ToneIsOnA
            te::convertPropertyToType<int> (frames, IDs::tb); // ToneIsOnB
            te::convertPropertyToType<int> (frames, IDs::tc); // ToneIsOnC
            te::convertPropertyToType<int> (frames, IDs::na); // NoiseIsOnA
            te::convertPropertyToType<int> (frames, IDs::nb); // NoiseIsOnB
            te::convertPropertyToType<int> (frames, IDs::nc); // NoiseIsOnC
            te::convertPropertyToType<int> (frames, IDs::ea); // EnvelopeIsOnA
            te::convertPropertyToType<int> (frames, IDs::eb); // EnvelopeIsOnB
            te::convertPropertyToType<int> (frames, IDs::ec); // EnvelopeIsOnC
            te::convertPropertyToType<int> (frames, IDs::ra); // RetriggerA
            te::convertPropertyToType<int> (frames, IDs::rb); // RetriggerB
            te::convertPropertyToType<int> (frames, IDs::rc); // RetriggerC
            te::convertPropertyToType<int> (frames, IDs::n);  // NoisePitch
            te::convertPropertyToType<int> (frames, IDs::e);  // EnvelopePitch
            te::convertPropertyToType<int> (frames, IDs::s);  // EnvelopeShape
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

juce::ValueTree PsgParamFrame::createPsgFrameValueTree(te::BeatPosition beat, const PsgParamFrameData& data) {
    auto v = te::createValueTree(IDs::FRAME,
        te::IDs::b,    roundTo(beat.inBeats())
    );
    PsgParamType::forEach([&v, &data](auto paramTypeVal) {
        auto paramType = paramTypeVal();  // Extract enum value from integral_constant
        if (data.isSet(paramType)) {
            switch(paramType) {
                case PsgParamType::VolumeA:           v.setProperty(IDs::va, *data[paramType], nullptr); break;
                case PsgParamType::VolumeB:           v.setProperty(IDs::vb, *data[paramType], nullptr); break;
                case PsgParamType::VolumeC:           v.setProperty(IDs::vc, *data[paramType], nullptr); break;
                case PsgParamType::TonePeriodA:       v.setProperty(IDs::pa, *data[paramType], nullptr); break;
                case PsgParamType::TonePeriodB:       v.setProperty(IDs::pb, *data[paramType], nullptr); break;
                case PsgParamType::TonePeriodC:       v.setProperty(IDs::pc, *data[paramType], nullptr); break;
                case PsgParamType::ToneIsOnA:         v.setProperty(IDs::ta, *data[paramType], nullptr); break;
                case PsgParamType::ToneIsOnB:         v.setProperty(IDs::tb, *data[paramType], nullptr); break;
                case PsgParamType::ToneIsOnC:         v.setProperty(IDs::tc, *data[paramType], nullptr); break;
                case PsgParamType::NoiseIsOnA:        v.setProperty(IDs::na, *data[paramType], nullptr); break;
                case PsgParamType::NoiseIsOnB:        v.setProperty(IDs::nb, *data[paramType], nullptr); break;
                case PsgParamType::NoiseIsOnC:        v.setProperty(IDs::nc, *data[paramType], nullptr); break;
                case PsgParamType::EnvelopeIsOnA:     v.setProperty(IDs::ea, *data[paramType], nullptr); break;
                case PsgParamType::EnvelopeIsOnB:     v.setProperty(IDs::eb, *data[paramType], nullptr); break;
                case PsgParamType::EnvelopeIsOnC:     v.setProperty(IDs::ec, *data[paramType], nullptr); break;
                case PsgParamType::RetriggerToneA:    v.setProperty(IDs::ra, *data[paramType], nullptr); break;
                case PsgParamType::RetriggerToneB:    v.setProperty(IDs::rb, *data[paramType], nullptr); break;
                case PsgParamType::RetriggerToneC:    v.setProperty(IDs::rc, *data[paramType], nullptr); break;
                case PsgParamType::RetriggerEnvelope: v.setProperty(IDs::re, *data[paramType], nullptr); break;
                case PsgParamType::NoisePeriod:       v.setProperty(IDs::n,  *data[paramType], nullptr); break;
                case PsgParamType::EnvelopePeriod:    v.setProperty(IDs::e,  *data[paramType], nullptr); break;
                case PsgParamType::EnvelopeShape:     v.setProperty(IDs::s,  *data[paramType], nullptr); break;
                default: break;
            }
        }
    });
    return v;
}

te::BeatPosition PsgParamFrame::getRawEditBeats(const PsgClip& c) const {
    return beatNumber - toDuration(c.getLoopStartBeats()) + toDuration(c.getContentStartBeat());
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

void PsgParamFrame::setBeatPosition(te::BeatPosition newBeatNumber, juce::UndoManager* um) {
    newBeatNumber = jmax(0_bp, newBeatNumber);

    if (beatNumber != newBeatNumber) {
        state.setProperty(te::IDs::b, newBeatNumber.inBeats(), um);
        beatNumber = newBeatNumber;
    }
}

void PsgParamFrame::setRawEditTime(const PsgClip& c, te::TimePosition editTime, juce::UndoManager* um) {
    const auto editBeat = c.edit.tempoSequence.toBeats(jmax(0_tp, editTime));
    setBeatPosition(editBeat + toDuration(c.getLoopStartBeats()) - toDuration(c.getContentStartBeat()), um);
}

void PsgParamFrame::updatePropertiesFromState() noexcept {
    beatNumber  = te::BeatPosition::fromBeats(static_cast<double>(state.getProperty(te::IDs::b)));
    // TODO update other properties
    // Why not use CahedValue<>? Too slow?
    // read all properties from state
    PsgParamType type = PsgParamType::undefined();
    for (int i = 0; i < state.getNumProperties(); ++i) {
        // TODO optimize somehow
        auto p = state.getPropertyName(i);
        if (p == IDs::va) {
            type = PsgParamType::VolumeA;
        } else if (p == IDs::vb) {
            type = PsgParamType::VolumeB;
        } else if (p == IDs::vc) {
            type = PsgParamType::VolumeC;
        } else if (p == IDs::pa) {
            type = PsgParamType::TonePeriodA;
        } else if (p == IDs::pb) {
            type = PsgParamType::TonePeriodB;
        } else if (p == IDs::pc) {
            type = PsgParamType::TonePeriodC;
        } else if (p == IDs::ta) {
            type = PsgParamType::ToneIsOnA;
        } else if (p == IDs::tb) {
            type = PsgParamType::ToneIsOnB;
        } else if (p == IDs::tc) {
            type = PsgParamType::ToneIsOnC;
        } else if (p == IDs::na) {
            type = PsgParamType::NoiseIsOnA;
        } else if (p == IDs::nb) {
            type = PsgParamType::NoiseIsOnB;
        } else if (p == IDs::nc) {
            type = PsgParamType::NoiseIsOnC;
        } else if (p == IDs::ea) {
            type = PsgParamType::EnvelopeIsOnA;
        } else if (p == IDs::eb) {
            type = PsgParamType::EnvelopeIsOnB;
        } else if (p == IDs::ec) {
            type = PsgParamType::EnvelopeIsOnC;
        } else if (p == IDs::ra) {
            type = PsgParamType::RetriggerToneA;
        } else if (p == IDs::rb) {
            type = PsgParamType::RetriggerToneB;
        } else if (p == IDs::rc) {
            type = PsgParamType::RetriggerToneC;
        } else if (p == IDs::re) {
            type = PsgParamType::RetriggerEnvelope;
        } else if (p == IDs::n) {
            type = PsgParamType::NoisePeriod;
        } else if (p == IDs::e) {
            type = PsgParamType::EnvelopePeriod;
        } else if (p == IDs::s) {
            type = PsgParamType::EnvelopeShape;
        } else {
            continue; // unknown property
        }
        if (type.isValid()) {
            data.set(type, static_cast<uint16>(static_cast<int>(state.getProperty(p))));
        }
    }
}

template<>
struct PsgList::EventDelegate<PsgParamFrame> {
    static bool isSuitableType(const juce::ValueTree& v) {
        return v.hasType(IDs::FRAME);
    }

    static bool updateObject(PsgParamFrame& e, const juce::Identifier& i) {
        e.updatePropertiesFromState();
        return i == te::IDs::b;
    }

    static void removeFromSelection(PsgParamFrame* e) {
        removePsgParamFromSelection(e);
    }
};

//==============================================================================
template<typename EventType>
const juce::Array<EventType*>& getEventsChecked(const juce::Array<EventType*>& events) {
    #if JUCE_DEBUG
        te::BeatPosition lastBeat;

        for (auto* e : events) {
            auto beat = e->getBeatPosition();
            jassert(lastBeat <= beat);
            lastBeat = beat;
        }
    #endif

    return events;
}

//==============================================================================
juce::ValueTree PsgList::createPsgList() {
    return createValueTree(IDs::PSG,
                           te::IDs::ver, 1,
                           te::IDs::channelNumber, te::MidiChannel(1));
}

PsgList::PsgList() : state (IDs::PSG) {
    state.setProperty(te::IDs::ver, 1, nullptr);
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
    framesPerBeat_.referTo(state, IDs::framesPerBeat, um, 0.0);

    framesList = std::make_unique<EventList<PsgParamFrame>>(state);

    if (canonicalizeFramesPerBeat(framesPerBeat_.get()) <= 0.0 && ! getFrames().isEmpty())
        setFramesPerBeat(getFramesPerBeatFromGeometry(getFrames()), um);

    recomputeAccumulatedState();
}

void PsgList::recomputeAccumulatedState() {
    ++dataVersion_;
    PsgParamFrameData accumulated;
    accumulated.resetMixer();
    for (auto* frame : getFrames()) {
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
    state.removeAllChildren(um);
    setFramesPerBeat(0.0, um);
    importedName = {};
}

void PsgList::copyFrom(const PsgList& other, juce::UndoManager* um) {
    if (this != &other) {
        clear(um);
        state.copyPropertiesFrom(other.state, um);
        addFrom(other, um);
    }
}

void PsgList::moveFrom(PsgList& other, juce::UndoManager* um) {
    if (this != &other) {
        clear(um);
        // Fastest way
        // TODO detach other from parent if any
        state.removeChild(state.getChildWithName(IDs::PSG), um);
        state.addChild(other.state, -1, um);
    }
}

void PsgList::addFrom(const PsgList& other, juce::UndoManager* um) {
    if (this != &other)
        for (int i = 0; i < other.state.getNumChildren(); ++i)
            state.addChild(other.state.getChild (i).createCopy(), -1, um);
}

void PsgList::setMidiChannel(te::MidiChannel newChannel) {
    midiChannel = newChannel;
}

void PsgList::setFramesPerBeat(double framesPerBeat, juce::UndoManager* um) {
    framesPerBeat_.setValue(canonicalizeFramesPerBeat(framesPerBeat), um);
}

PsgParamFrame* PsgList::addFrameEvent(const PsgParamFrame& event, juce::UndoManager* um) {
    auto v = event.state.createCopy();
    state.addChild(v, -1, um);
    recomputeAccumulatedState();
    return framesList->getEventFor(v);
}

PsgParamFrame* PsgList::addFrameEvent(te::BeatPosition beat, const PsgParamFrameData& data, juce::UndoManager* um) {
    auto v = PsgParamFrame::createPsgFrameValueTree(beat, data);
    state.addChild(v, -1, um);
    recomputeAccumulatedState();
    return framesList->getEventFor(v);
}

PsgParamFrame* PsgList::addFrameEvent(te::BeatPosition beat, const uZX::PsgRegsFrame& regs, juce::UndoManager* um) {
    auto v = PsgParamFrame::createPsgFrameValueTree(beat, PsgParamFrameData {regs});
    state.addChild(v, -1, um);
    recomputeAccumulatedState();
    return framesList->getEventFor(v);
}

void PsgList::removeFrameEvent(PsgParamFrame& e, juce::UndoManager* um) {
    state.removeChild(e.state, um);
}

void PsgList::removeAllFrames(juce::UndoManager* um) {
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild(i).hasType(IDs::FRAME))
            state.removeChild(i, um);
}

void PsgList::loadFrom(const uZX::PsgData &data, te::Edit& edit, juce::UndoManager* um) {
    PsgParamFrameData params;
    uZX::PsgRegsFrame regsState;
    // uZX::PsgRegsFrame regsFromParamsState;
    params.resetMixer();  // because AY regs after reset has all NNNTTT flags set (bits==0)

    // Track last envelope shape and retrigger state for state transitions
    std::optional<uint8_t> lastEnvelopeShape;
    bool lastRetriggerState = false;  // Track if last frame had retrigger=1

    setFramesPerBeat(data.getFrameRate() * getBeatLengthSecondsAt(edit, 0_tp), um);
    // Adopt the imported file's frame rate as the project fps so the transport
    // readout and the grid/ruler reflect the material being edited.
    Helpers::setProjectFps(edit, data.getFrameRate());

    for (size_t i = 0; i < data.frames.size(); i++) {
        auto &frame = data.frames[i];
        if (frame.isEmpty()) {
            continue;
        }
        auto timeSec = data.frameNumToSeconds(i);
        auto beat = edit.tempoSequence.toBeats(te::TimePosition::fromSeconds(timeSec));
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

        auto v = PsgParamFrame::createPsgFrameValueTree(beat, params);
        state.addChild(v, -1, um);
    }

    recomputeAccumulatedState();
}

static double getFramesPerBeatFromGeometry(const juce::Array<PsgParamFrame*>& frames) {
    // Legacy backfill only: infer from the smallest positive stored beat gap and persist
    // the result as framesPerBeat. New imports store dense frames-per-beat directly.
    te::BeatDuration minGap;
    bool found = false;
    for (int i = 1; i < frames.size(); ++i) {
        const auto gap = frames[i]->getBeatPosition() - frames[i - 1]->getBeatPosition();
        if (gap > te::BeatDuration() && (! found || gap < minGap)) {
            minGap = gap;
            found = true;
        }
    }

    if (! found || minGap.inBeats() <= 0.0)
        return 0.0;  // fewer than two distinct frames -> unknown

    return canonicalizeFramesPerBeat(std::max(1.0, (double) roundToInt(1.0 / minGap.inBeats())));
}

double PsgList::getFramesPerBeat() const {
    return canonicalizeFramesPerBeat(framesPerBeat_.get());
}

double PsgList::getEffectiveFps(const PsgClip& clip) const {
    const auto framesPerBeat = getFramesPerBeat();
    const auto beatLength = getBeatLengthSecondsAt(clip.edit, clip.getPosition().getStart());
    if (framesPerBeat <= 0.0 || beatLength <= 0.0)
        return 0.0;  // can't measure (fewer than two frames) -> no meaningful rate

    return framesPerBeat / beatLength;
}

bool PsgList::hasFpsMismatch(const PsgClip& clip, double editFps) const {
    const auto effective = getEffectiveFps(clip);
    if (effective <= 0.0 || editFps <= 0.0)
        return false;

    // Small relative tolerance so float noise never trips the warning while a genuine
    // rate difference still does. 0.1% is far below the integer-rounded chip's precision.
    const auto tolerance = 1.0e-3 * jmax(effective, editFps);
    return std::abs(effective - editFps) > tolerance;
}

void PsgList::loadFrom(const uZX::PsgFile &psgFile, te::Edit& edit, juce::UndoManager* um) {
    loadFrom(psgFile.getData(), edit, um);
    if (psgFile.getFile().existsAsFile()) {
        importedName = psgFile.getFile().getFileNameWithoutExtension();
        importedFileName = psgFile.getFile().getFullPathName();
    } else {
        importedName = {};
        importedFileName = {};
    }
}

const juce::Array<PsgParamFrame*>& PsgList::getFrames() const {
    jassert (framesList != nullptr);
    return getEventsChecked(framesList->getSortedList());
}

//==============================================================================
te::BeatPosition PsgList::getFirstBeatNumber() const {
    auto t = te::BeatPosition::fromBeats(te::Edit::maximumLength);
    if (auto first = getFrames().getFirst())  t = std::min(t, first->getBeatPosition());
    return t;
}

te::BeatPosition PsgList::getLastBeatNumber() const {
    te::BeatPosition t;
    if (auto last = getFrames().getLast())  t = std::max (t, last->getBeatPosition());
    return t;
}

const PsgParamFrame* PsgList::getFrameAt(te::BeatPosition beat) const {
    const auto& frames = getFrames();
    if (frames.isEmpty())
        return nullptr;

    // Binary search for exact beat position
    int low = 0;
    int high = frames.size() - 1;

    while (low <= high) {
        int mid = (low + high) / 2;
        auto midBeat = frames[mid]->getBeatPosition();

        if (midBeat == beat)
            return frames[mid];
        else if (midBeat < beat)
            low = mid + 1;
        else
            high = mid - 1;
    }

    return nullptr;
}

double PsgList::getTimeInBase(const PsgParamFrame& frame, PsgClip& clip, te::MidiList::TimeBase timeBase) const {
    switch (timeBase) {
        case te::MidiList::TimeBase::beatsRaw:
            return frame.getBeatPosition().inBeats();
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
