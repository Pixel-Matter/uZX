#pragma once

#include <JuceHeader.h>


namespace MoTool::uZX {

//==============================================================================
/**
    MIDI effect is a simple functor

    Any MIDI effect process MIDI events in passed context.
    Just call it with a MidiBufferContext object as a functor.
*/
struct MidiBufferContext {
    // MIDI buffer to process or output.
    tracktion::MidiMessageArray& buffer;

    // Start sample of the buffer to process.
    // MIDI buffer can contain events prior to this sample, ignore them.
    const int start = 0;

    // Length of the buffer to process in samples.
    // MIDI buffer can contain events beyond this length, ignore them.
    const int length;

    // Position in the edit (or just a free running time) of the buffer start
    const tracktion::TimePosition playPosition { };

    // Sample rate for converting time to sample numbers and back
    const double sampleRate = 0.0f;

    inline bool isAllNotesOff() const noexcept {
        return buffer.isAllNotesOff;
    }

    // Create a new context with the same buffer but sliced to the given range
    inline MidiBufferContext sliced(int startSample, int numSamples) const {
        return { buffer, startSample, numSamples, playPosition, sampleRate };
    }

    // Conver time in seconds relative to the start of the buffer to samples
    inline int getSampleForTimeRel(double time) const noexcept {
        jassert(sampleRate > 0.0);
        return juce::roundToInt(time * sampleRate);
    }

    inline int getSampleForTimeRel(tracktion::TimePosition time) const noexcept {
        return getSampleForTimeRel(time.inSeconds());
    }

    inline int getSampleForTimeRel(tracktion::TimeDuration time) const noexcept {
        return getSampleForTimeRel(time.inSeconds());
    }

    // Start time to process, relative to the start of the whole buffer in seconds
    inline tracktion::TimeDuration processStartTimeRel() const noexcept {
        return tracktion::TimeDuration::fromSeconds(start / sampleRate);
    }

    inline tracktion::TimePosition processStartTime() const noexcept {
        return playPosition + processStartTimeRel();
    }

    inline tracktion::TimeDuration duration() const noexcept {
        return tracktion::TimeDuration::fromSeconds(length / sampleRate);
    }

    inline tracktion::TimePosition processEndTime() const noexcept {
        return processStartTime() + duration();
    }

    inline double toOffset(tracktion::TimePosition time) const noexcept {
        return (time - playPosition).inSeconds();
    }

    void debugMidiBuffer() const {
        for (const auto& m : buffer) {
            auto sample = getSampleForTimeRel(m.getTimeStamp());
            if (sample < start || sample >= start + length) {
                DBG("Midi message out of range: " << m.getDescription()
                    << " at " << m.getTimeStamp() + playPosition.inSeconds()
                    << " local " << m.getTimeStamp());
            } else {
                DBG(m.getDescription()
                    << " at " << m.getTimeStamp() + playPosition.inSeconds()
                    << " local " << m.getTimeStamp());
            }
        }
    }

};

//==============================================================================
/**
    C++20 concept instead of inheritance
*/
template<typename T>
concept MidiEffectConcept = requires(T& effect, MidiBufferContext& context) {
    // Main processing function - must be callable as functor
    effect(context);

    // TODO should this class have following methods?
    // initialize
    // reset
    // midipanic
    // bypass(bool on)

    /**
    Optional: type can have additional methods but concept doesn't require them
    This gives maximum flexibility for different effect implementations

    Usage example:

    template<MidiEffect Effect>
    void processMidiWithEffect(Effect& effect, MidiBufferContext& context) {
        effect(context);  // Just call it as a functor
    }
    */
};


//==============================================================================
/**
    Base class for virtual inheritance (but try to use a concept above instead)
    You should use it if you need dynamic fx
    If you need MIDI effects registry, do it separately,
    derive from this class and add a name. But also you have Plugins for that
*/
class MidiEffectBase {
public:
    MidiEffectBase() = default;
    virtual ~MidiEffectBase() = default;

    // Main processing function
    virtual void operator()(MidiBufferContext& fc) = 0;

    // TODO should this class have following methods?
    // initialize
    // reset
    // midipanic
    // bypass(bool on)

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEffectBase)
};


//==============================================================================
/**
    Chaining MIDI effects

    TODO
    - How to index effects in the chain?
*/
class MidiEffectChain : public MidiEffectBase {
public:
    // Main processing function
    void operator()(MidiBufferContext& fc) override {
        ScopedLock sl(lock);
        for (auto& effect : effects) {
            if (effect) {
                (*effect)(fc);
            }
        }
    }

    void add(std::unique_ptr<MidiEffectBase> effect) {
        ScopedLock sl(lock);
        effects.push_back(std::move(effect));
    }

    void insert(std::unique_ptr<MidiEffectBase> effect, size_t index) {
        ScopedLock sl(lock);
        if (index < effects.size()) {
            effects.insert(effects.begin() + (int) index, std::move(effect));
        } else {
            effects.push_back(std::move(effect));
        }
    }

    void remove(MidiEffectBase* effect) {
        ScopedLock sl(lock);
        effects.erase(std::remove_if(effects.begin(), effects.end(),
            [effect](const auto& ptr) { return ptr.get() == effect; }),
            effects.end());
    }

    size_t size() const {
        ScopedLock sl(lock);
        return effects.size();
    }

private:
    CriticalSection lock;
    std::vector<std::unique_ptr<MidiEffectBase>> effects;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEffectChain)
};


//==============================================================================
/**
    Base class for MIDI effect Plugin.
    MIDI in — MIDI out only.
    Not a synth!

    TODO
    - mechanism for static and dynamic parameter bindings and registration.
*/
template <MidiEffectConcept MIDIFX>
class MidiFxPluginBase : public tracktion::Plugin {
public:

    MidiFxPluginBase(tracktion::PluginCreationInfo info, MIDIFX& fx)
        : tracktion::Plugin(info)
        , midiEffect(fx)
    {}

    enum class PositionSource {
        Edit,
        Emulated
    };

    using tracktion::Plugin::Plugin;

    String getVendor() override { return "PixelMatter"; }
    double getLatencySeconds() override { return 0.0; }
    int getNumOutputChannelsGivenInputs(int) override { return 0; }
    void getChannelNames(juce::StringArray*, juce::StringArray*) override {}
    bool takesAudioInput() override { return false; }
    bool canBeAddedToClip() override { return false; }
    bool takesMidiInput() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return false; }

    void initialise(const tracktion::PluginInitialisationInfo& /*info*/) override {
        // setCurrentPlaybackSampleRate(info.sampleRate);
    }

    void deinitialise() override {}

    void applyToBuffer(const tracktion::PluginRenderContext& fc) override {
        if (fc.bufferForMidiMessages == nullptr)
            return;

        jassert(fc.bufferStartSample == 0);
        jassert(fc.midiBufferOffset == 0.0);

        if (!fc.editTime.isEmpty()) {
            positionSource = PositionSource::Edit;
            playPosition = fc.editTime.getStart();
        } else {  // not playing along Edit
            if (positionSource != PositionSource::Emulated) {
                positionSource = PositionSource::Emulated;
                // restart position to be emulated
                playPosition = fc.editTime.getStart();
            }
        }

        // TODO add current tempo to that because when playingWhileStopped == true
        //      we can not deterime tempo from tempoSequence
        // DBG("Playing at " << playPosition.inSeconds() << "s");
        MidiBufferContext context {
            .buffer = *fc.bufferForMidiMessages,
            .start = fc.bufferStartSample,
            .length = fc.bufferNumSamples,
            .playPosition = playPosition,
            .sampleRate = sampleRate
        };

        midiEffect(context);

        if (positionSource == PositionSource::Emulated) {
            playPosition = playPosition + context.duration();
        }
    }

protected:
    MIDIFX& midiEffect;

private:
    PositionSource positionSource = PositionSource::Emulated;
    tracktion::TimePosition playPosition {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiFxPluginBase)
};

}  // namespace MoTool::uZX
