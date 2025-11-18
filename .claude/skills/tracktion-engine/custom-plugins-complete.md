# Custom Plugin Development - Complete Guide

Comprehensive guide to developing custom plugins for Tracktion Engine, including effects, instruments, and MIDI processors.

## Overview

Tracktion Engine's plugin system allows you to create:
- **Audio Effects**: Process audio buffers (EQ, compression, reverb, etc.)
- **MIDI Effects**: Transform MIDI messages (arpeggiators, chord generators, etc.)
- **Instruments**: Generate audio from MIDI (synthesizers, samplers, etc.)
- **Utility Plugins**: Special-purpose processors (meters, analyzers, etc.)

## Core Plugin Architecture

### Plugin Base Class

```cpp
class MyPlugin : public te::Plugin
{
public:
    // Constructor receives creation info
    MyPlugin(te::PluginCreationInfo info)
        : Plugin(info)
    {
        // Initialize parameters
        initializeParameters();
    }

    // Required: Unique XML type name
    static const char* xmlTypeName;

    // Required: Display name
    static const char* getPluginName() { return "My Plugin"; }

    // Override: Return display name
    juce::String getName() const override { return getPluginName(); }

    // Override: Return XML type
    juce::String getPluginType() override { return xmlTypeName; }

    // Override: Plugin identifier (for preset management)
    juce::String getIdentifierString() override { return xmlTypeName; }

    // Override: Initialization when added to track
    void initialise(const PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        // Setup DSP, allocate buffers
    }

    // Override: Cleanup
    void deinitialise() override
    {
        // Release resources
    }

    // Override: Process audio
    void applyToBuffer(const PluginRenderContext& prc) override
    {
        // Process audio buffer
    }

    // Override: Return automatable parameters
    juce::ReferenceCountedArray<AutomatableParameter> getAutomatableParameters() override
    {
        return parameters;
    }

    // Override: State persistence
    void restorePluginStateFromValueTree(const juce::ValueTree& v) override
    {
        // Restore parameters from ValueTree
    }

protected:
    double sampleRate = 44100.0;
    juce::ReferenceCountedArray<AutomatableParameter> parameters;

private:
    void initializeParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyPlugin)
};

// Define XML type
const char* MyPlugin::xmlTypeName = "myPlugin";
```

## Parameter System

### Creating Parameters

```cpp
void MyPlugin::initializeParameters()
{
    auto um = getUndoManager();

    // Continuous parameter with range
    gainParam = addParam("gain", "Gain",
        { -24.0f, 24.0f },                        // Range
        [] (float v) { return juce::String(v, 1) + " dB"; },  // Value to text
        [] (const juce::String& s) { return s.getFloatValue(); });  // Text to value

    // Attach to cached value for automatic sync
    gainParam->attachToCurrentValue(gainDb);

    // Choice parameter (dropdown)
    modeParam = addParam("mode", "Mode",
        { 0.0f, 2.0f },
        [] (float v) {
            int mode = (int)v;
            return mode == 0 ? "Soft" : mode == 1 ? "Medium" : "Hard";
        },
        [] (const juce::String& s) {
            return s == "Soft" ? 0.0f : s == "Medium" ? 1.0f : 2.0f;
        });

    // Boolean parameter
    bypassParam = addParam("bypass", "Bypass",
        { 0.0f, 1.0f },
        [] (float v) { return v > 0.5f ? "On" : "Off"; },
        [] (const juce::String& s) { return s == "On" ? 1.0f : 0.0f; });

    bypassParam->attachToCurrentValue(bypassEnabled);

    // Store in array for automation
    parameters.add(gainParam);
    parameters.add(modeParam);
    parameters.add(bypassParam);
}
```

### Parameter Properties

```cpp
// Set parameter metadata
gainParam->setParameterName("Gain");
gainParam->setParameterShortName("Gain");

// Default value
gainParam->setParameter(0.0f, juce::sendNotification);

// Normalized range (0-1)
gainParam->setNormalisedParameter(0.5f);  // Middle of range

// Get current value
float currentGain = gainParam->getCurrentValue();

// Automation
gainParam->setAutomationActive(true);
auto& curve = gainParam->getCurve();

// MIDI learn
gainParam->midiLearnMode = true;
```

### State Persistence

```cpp
void MyPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v)
{
    // Option 1: Automatic with CachedValue
    juce::CachedValue<float>* cvsFloat[] = { &gainDb, &threshold, nullptr };
    juce::CachedValue<bool>* cvsBool[] = { &bypassEnabled, nullptr };
    juce::CachedValue<int>* cvsInt[] = { &mode, nullptr };

    copyPropertiesToCachedValues(v, cvsFloat);
    copyPropertiesToCachedValues(v, cvsBool);
    copyPropertiesToCachedValues(v, cvsInt);

    // Option 2: Manual
    gainDb = v.getProperty("gain", 0.0f);
    bypassEnabled = v.getProperty("bypass", false);
}
```

## Audio Effect Plugin

### Complete Example: Simple Gain Effect

```cpp
class SimpleGainPlugin : public te::Plugin
{
public:
    SimpleGainPlugin(te::PluginCreationInfo info)
        : Plugin(info)
    {
        auto um = getUndoManager();

        gainParam = addParam("gain", "Gain",
            { -60.0f, 12.0f },
            [] (float v) { return juce::String(v, 1) + " dB"; },
            [] (const juce::String& s) { return s.getFloatValue(); });

        gainParam->attachToCurrentValue(gainDb);
        parameters.add(gainParam);
    }

    static const char* xmlTypeName;
    static const char* getPluginName() { return "Simple Gain"; }

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }

    void initialise(const PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        smoother.reset(sampleRate, 0.05);  // 50ms smoothing
    }

    void applyToBuffer(const PluginRenderContext& prc) override
    {
        if (prc.destBuffer.getNumChannels() == 0)
            return;

        // Get target gain (respects automation)
        float targetGainDb = gainParam->getCurrentValue();
        float targetLinearGain = juce::Decibels::decibelsToGain(targetGainDb);

        smoother.setTargetValue(targetLinearGain);

        // Apply with smoothing
        for (int ch = 0; ch < prc.destBuffer.getNumChannels(); ++ch)
        {
            auto* samples = prc.destBuffer.getWritePointer(ch, prc.bufferStartSample);

            for (int i = 0; i < prc.bufferNumSamples; ++i)
                samples[i] *= smoother.getNextValue();
        }
    }

    juce::ReferenceCountedArray<AutomatableParameter> getAutomatableParameters() override
    {
        return parameters;
    }

    void restorePluginStateFromValueTree(const juce::ValueTree& v) override
    {
        juce::CachedValue<float>* cvsFloat[] = { &gainDb, nullptr };
        copyPropertiesToCachedValues(v, cvsFloat);
    }

private:
    double sampleRate = 44100.0;
    AutomatableParameter::Ptr gainParam;
    juce::CachedValue<float> gainDb { 0.0f };
    juce::ReferenceCountedArray<AutomatableParameter> parameters;
    juce::SmoothedValue<float> smoother;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleGainPlugin)
};

const char* SimpleGainPlugin::xmlTypeName = "simpleGain";
```

### Advanced Audio Effect with DSP

```cpp
class SimpleEQPlugin : public te::Plugin
{
public:
    SimpleEQPlugin(te::PluginCreationInfo info)
        : Plugin(info)
    {
        // Create parameters for each band
        lowGainParam = addParam("lowGain", "Low Gain",
            { -24.0f, 24.0f },
            [] (float v) { return juce::String(v, 1) + " dB"; },
            [] (const juce::String& s) { return s.getFloatValue(); });

        midFreqParam = addParam("midFreq", "Mid Frequency",
            { 200.0f, 5000.0f },
            [] (float v) { return juce::String((int)v) + " Hz"; },
            [] (const juce::String& s) { return s.getFloatValue(); });

        lowGainParam->attachToCurrentValue(lowGain);
        midFreqParam->attachToCurrentValue(midFreq);

        parameters.add(lowGainParam);
        parameters.add(midFreqParam);
    }

    static const char* xmlTypeName;
    static const char* getPluginName() { return "Simple EQ"; }

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }

    void initialise(const PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;

        // Prepare filter chain
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = 512;
        spec.numChannels = 2;

        filterChain.prepare(spec);
        updateFilters();
    }

    void deinitialise() override
    {
        filterChain.reset();
    }

    void applyToBuffer(const PluginRenderContext& prc) override
    {
        if (prc.destBuffer.getNumChannels() == 0)
            return;

        // Update filters if parameters changed
        if (needsUpdate.exchange(false))
            updateFilters();

        // Process with JUCE DSP
        juce::dsp::AudioBlock<float> block(prc.destBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        filterChain.process(context);
    }

    juce::ReferenceCountedArray<AutomatableParameter> getAutomatableParameters() override
    {
        return parameters;
    }

    void restorePluginStateFromValueTree(const juce::ValueTree& v) override
    {
        juce::CachedValue<float>* cvs[] = { &lowGain, &midFreq, nullptr };
        copyPropertiesToCachedValues(v, cvs);
        needsUpdate = true;
    }

private:
    void updateFilters()
    {
        auto& lowShelf = filterChain.get<0>();
        auto& peak = filterChain.get<1>();

        *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sampleRate, 100.0f, 0.7f, juce::Decibels::decibelsToGain(lowGain.get()));

        *peak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, midFreq.get(), 1.0f, juce::Decibels::decibelsToGain(0.0f));
    }

    using FilterChain = juce::dsp::ProcessorChain<
        juce::dsp::IIR::Filter<float>,  // Low shelf
        juce::dsp::IIR::Filter<float>   // Peak
    >;

    double sampleRate = 44100.0;
    FilterChain filterChain;
    std::atomic<bool> needsUpdate { false };

    AutomatableParameter::Ptr lowGainParam, midFreqParam;
    juce::CachedValue<float> lowGain { 0.0f }, midFreq { 1000.0f };
    juce::ReferenceCountedArray<AutomatableParameter> parameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleEQPlugin)
};

const char* SimpleEQPlugin::xmlTypeName = "simpleEQ";
```

## MIDI Effect Plugin

### Basic MIDI Processor

```cpp
class MidiTransposePlugin : public te::Plugin
{
public:
    MidiTransposePlugin(te::PluginCreationInfo info)
        : Plugin(info)
    {
        transposeParam = addParam("transpose", "Transpose",
            { -24.0f, 24.0f },
            [] (float v) { return juce::String((int)v) + " semi"; },
            [] (const juce::String& s) { return s.getFloatValue(); });

        transposeParam->attachToCurrentValue(transposeSemitones);
        parameters.add(transposeParam);
    }

    static const char* xmlTypeName;
    static const char* getPluginName() { return "MIDI Transpose"; }

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }

    void applyToBufferWithAutomation(const PluginRenderContext& prc) override
    {
        // Process MIDI
        if (prc.bufferForMidiMessages != nullptr)
        {
            auto& midiBuffer = *prc.bufferForMidiMessages;
            juce::MidiBuffer processedMidi;

            int transpose = (int)transposeSemitones.get();

            for (auto metadata : midiBuffer)
            {
                auto msg = metadata.getMessage();

                if (msg.isNoteOnOrOff())
                {
                    int newNote = juce::jlimit(0, 127, msg.getNoteNumber() + transpose);
                    msg.setNoteNumber(newNote);
                }

                processedMidi.addEvent(msg, metadata.samplePosition);
            }

            midiBuffer.swapWith(processedMidi);
        }
    }

    juce::ReferenceCountedArray<AutomatableParameter> getAutomatableParameters() override
    {
        return parameters;
    }

    void restorePluginStateFromValueTree(const juce::ValueTree& v) override
    {
        juce::CachedValue<float>* cvs[] = { &transposeSemitones, nullptr };
        copyPropertiesToCachedValues(v, cvs);
    }

private:
    AutomatableParameter::Ptr transposeParam;
    juce::CachedValue<float> transposeSemitones { 0.0f };
    juce::ReferenceCountedArray<AutomatableParameter> parameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiTransposePlugin)
};

const char* MidiTransposePlugin::xmlTypeName = "midiTranspose";
```

### Advanced MIDI Effect: Arpeggiator

```cpp
class ArpeggiatorPlugin : public te::Plugin
{
public:
    ArpeggiatorPlugin(te::PluginCreationInfo info)
        : Plugin(info)
    {
        rateParam = addParam("rate", "Rate",
            { 0.0f, 3.0f },
            [] (float v) {
                int rate = (int)v;
                return rate == 0 ? "1/16" : rate == 1 ? "1/8" : rate == 2 ? "1/4" : "1/2";
            },
            [] (const juce::String& s) {
                return s == "1/16" ? 0.0f : s == "1/8" ? 1.0f : s == "1/4" ? 2.0f : 3.0f;
            });

        rateParam->attachToCurrentValue(rate);
        parameters.add(rateParam);
    }

    static const char* xmlTypeName;
    static const char* getPluginName() { return "Arpeggiator"; }

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }

    void applyToBufferWithAutomation(const PluginRenderContext& prc) override
    {
        if (prc.bufferForMidiMessages == nullptr)
            return;

        auto& midiBuffer = *prc.bufferForMidiMessages;

        // Track held notes
        for (auto metadata : midiBuffer)
        {
            auto msg = metadata.getMessage();

            if (msg.isNoteOn())
                heldNotes.addSorted(msg.getNoteNumber());
            else if (msg.isNoteOff())
                heldNotes.removeAllInstancesOf(msg.getNoteNumber());
        }

        // Generate arpeggiated notes
        if (!heldNotes.isEmpty())
        {
            juce::MidiBuffer arpMidi;
            generateArpeggio(arpMidi, prc.bufferNumSamples);
            midiBuffer.swapWith(arpMidi);
        }
    }

    juce::ReferenceCountedArray<AutomatableParameter> getAutomatableParameters() override
    {
        return parameters;
    }

    void restorePluginStateFromValueTree(const juce::ValueTree& v) override
    {
        juce::CachedValue<float>* cvs[] = { &rate, nullptr };
        copyPropertiesToCachedValues(v, cvs);
    }

private:
    void generateArpeggio(juce::MidiBuffer& buffer, int numSamples)
    {
        // Implementation: generate arpeggiated pattern
        // Based on rate and held notes
    }

    AutomatableParameter::Ptr rateParam;
    juce::CachedValue<float> rate { 1.0f };
    juce::ReferenceCountedArray<AutomatableParameter> parameters;
    juce::Array<int> heldNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpeggiatorPlugin)
};

const char* ArpeggiatorPlugin::xmlTypeName = "arpeggiator";
```

## Synthesizer Plugin

### Basic Synth with JUCE Synthesiser

```cpp
class SimpleSynthPlugin : public te::Plugin
{
public:
    SimpleSynthPlugin(te::PluginCreationInfo info)
        : Plugin(info)
    {
        // Add voices
        for (int i = 0; i < 8; ++i)
            synth.addVoice(new SimpleSynthVoice());

        synth.addSound(new SimpleSynthSound());

        // Parameters
        attackParam = addParam("attack", "Attack",
            { 0.01f, 2.0f },
            [] (float v) { return juce::String(v, 2) + " s"; },
            [] (const juce::String& s) { return s.getFloatValue(); });

        attackParam->attachToCurrentValue(attack);
        parameters.add(attackParam);
    }

    static const char* xmlTypeName;
    static const char* getPluginName() { return "Simple Synth"; }

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }

    void initialise(const PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void applyToBufferWithAutomation(const PluginRenderContext& prc) override
    {
        // Update voices with parameter values
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto voice = dynamic_cast<SimpleSynthVoice*>(synth.getVoice(i)))
                voice->setAttack(attack.get());
        }

        // Process MIDI and generate audio
        if (prc.bufferForMidiMessages != nullptr)
        {
            synth.renderNextBlock(prc.destBuffer,
                                 *prc.bufferForMidiMessages,
                                 prc.bufferStartSample,
                                 prc.bufferNumSamples);
        }
    }

    juce::ReferenceCountedArray<AutomatableParameter> getAutomatableParameters() override
    {
        return parameters;
    }

    void restorePluginStateFromValueTree(const juce::ValueTree& v) override
    {
        juce::CachedValue<float>* cvs[] = { &attack, nullptr };
        copyPropertiesToCachedValues(v, cvs);
    }

private:
    class SimpleSynthSound : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote(int) override { return true; }
        bool appliesToChannel(int) override { return true; }
    };

    class SimpleSynthVoice : public juce::SynthesiserVoice
    {
    public:
        bool canPlaySound(juce::SynthesiserSound*) override { return true; }

        void setAttack(float newAttack) { attackTime = newAttack; }

        void startNote(int midiNoteNumber, float velocity,
                      juce::SynthesiserSound*, int) override
        {
            frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
            phase = 0.0;
            level = velocity;

            env.attack(attackTime);
        }

        void stopNote(float, bool allowTailOff) override
        {
            env.release(0.1f);
        }

        void renderNextBlock(juce::AudioBuffer<float>& buffer,
                           int startSample, int numSamples) override
        {
            for (int i = 0; i < numSamples; ++i)
            {
                auto sample = std::sin(phase * juce::MathConstants<float>::twoPi);
                sample *= env.process() * level;

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.addSample(ch, startSample + i, sample);

                phase += frequency / getSampleRate();
                if (phase >= 1.0)
                    phase -= 1.0;
            }
        }

    private:
        double phase = 0.0, frequency = 440.0;
        float level = 0.0f, attackTime = 0.1f;
        juce::ADSR env;
    };

    double sampleRate = 44100.0;
    juce::Synthesiser synth;
    AutomatableParameter::Ptr attackParam;
    juce::CachedValue<float> attack { 0.1f };
    juce::ReferenceCountedArray<AutomatableParameter> parameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleSynthPlugin)
};

const char* SimpleSynthPlugin::xmlTypeName = "simpleSynth";
```

## Plugin Registration and Initialization

### Registering Plugins with Engine

```cpp
// In your application initialization (e.g., Main.cpp or Engine setup)
void registerCustomPlugins()
{
    // Register each custom plugin type
    te::Plugin::registerPluginType<SimpleGainPlugin>();
    te::Plugin::registerPluginType<SimpleEQPlugin>();
    te::Plugin::registerPluginType<MidiTransposePlugin>();
    te::Plugin::registerPluginType<ArpeggiatorPlugin>();
    te::Plugin::registerPluginType<SimpleSynthPlugin>();
}

// Call during engine initialization
int main()
{
    te::Engine engine("MyApp");

    // Register custom plugins
    registerCustomPlugins();

    // ... rest of application
}
```

### Creating Plugin Instances

```cpp
// Insert custom plugin on track
auto track = edit.getAudioTracks()[0];

// By XML type name
auto plugin = track->pluginList.insertPlugin(
    te::Plugin::create<SimpleGainPlugin>(), -1);

// Or using identifier
auto plugin2 = track->pluginList.insertPlugin(
    te::Plugin::create(SimpleGainPlugin::xmlTypeName), -1);

// Cast to specific type for access
if (auto gainPlugin = dynamic_cast<SimpleGainPlugin*>(plugin))
{
    // Access plugin-specific methods
}
```

## Plugin UI (Editor Component)

### Creating Custom Editor

```cpp
class MyPluginEditor : public te::Plugin::EditorComponent
{
public:
    MyPluginEditor(te::Plugin& p, MyPlugin& mp)
        : EditorComponent(p), myPlugin(mp)
    {
        // Create UI components
        gainSlider.setRange(-60.0, 12.0);
        gainSlider.setValue(myPlugin.getGainDb());
        gainSlider.onValueChange = [this]
        {
            myPlugin.setGainDb(gainSlider.getValue());
        };

        addAndMakeVisible(gainSlider);
        setSize(300, 200);
    }

    void resized() override
    {
        gainSlider.setBounds(getLocalBounds().reduced(20));
    }

private:
    MyPlugin& myPlugin;
    juce::Slider gainSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyPluginEditor)
};

// In plugin class:
juce::Component* MyPlugin::createEditor() override
{
    return new MyPluginEditor(*this, *this);
}
```

## Best Practices

1. **Thread Safety**: Never allocate/deallocate in `applyToBuffer()`
2. **Smoothing**: Use `juce::SmoothedValue` for parameter changes
3. **Automation**: Always use `getCurrentValue()` to respect automation
4. **State**: Store all parameters in ValueTree via `CachedValue`
5. **Initialization**: Setup DSP in `initialise()`, not constructor
6. **Registration**: Register plugins before creating Edit instances
7. **XML Type**: Use unique, descriptive XML type names
8. **Parameters**: Provide clear text conversion functions
9. **Latency**: Report latency via `getLatencySamples()` if applicable
10. **Editor**: Keep UI in separate class from plugin logic

## Common Patterns

### Parameter Smoothing

```cpp
class SmoothedPlugin : public te::Plugin
{
    void applyToBuffer(const PluginRenderContext& prc) override
    {
        smoother.setTargetValue(param->getCurrentValue());

        for (int i = 0; i < numSamples; ++i)
        {
            float value = smoother.getNextValue();
            // Use smoothed value
        }
    }

private:
    juce::SmoothedValue<float> smoother;
};
```

### Per-Sample Automation

```cpp
void applyToBuffer(const PluginRenderContext& prc) override
{
    for (int i = 0; i < prc.bufferNumSamples; ++i)
    {
        // Get automation value at exact sample
        double time = prc.editTime + i / sampleRate;
        float value = param->getCurveValue(time);

        // Process with per-sample accuracy
    }
}
```
