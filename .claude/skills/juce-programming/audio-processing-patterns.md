# JUCE Audio Processing Patterns

Advanced patterns and best practices for audio processing in JUCE.

## Audio Processor Lifecycle

### Initialization Order

1. **Constructor**: Initialize member variables, create parameters
2. **prepareToPlay()**: Allocate buffers, initialize DSP with sample rate
3. **processBlock()**: Real-time audio processing
4. **releaseResources()**: Clean up audio resources
5. **Destructor**: Final cleanup

### Example Audio Processor Class

```cpp
class MyAudioProcessor : public juce::AudioProcessor
{
public:
    MyAudioProcessor()
        : AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo())
            .withOutput("Output", juce::AudioChannelSet::stereo()))
    {
        // Initialize parameters
        addParameter(gain = new juce::AudioParameterFloat(
            "gain", "Gain", 0.0f, 1.0f, 0.5f));
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        // Initialize DSP with sample rate and channels
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
        spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

        processorChain.prepare(spec);

        // Allocate any buffers needed
        // Reset state
    }

    void processBlock(juce::AudioBuffer<float>& buffer,
                     juce::MidiBuffer& midiMessages) override
    {
        // Always clear unused channels
        for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear(ch, 0, buffer.getNumSamples());

        // Create DSP block
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        // Process
        processorChain.process(context);
    }

    void releaseResources() override
    {
        // Release audio resources
        processorChain.reset();
    }

private:
    juce::AudioParameterFloat* gain;

    using ProcessorChain = juce::dsp::ProcessorChain<
        juce::dsp::Gain<float>,
        juce::dsp::StateVariableTPTFilter<float>
    >;
    ProcessorChain processorChain;
};
```

---

## Real-Time Safety

### Rules for processBlock()

**NEVER do these in processBlock()**:
```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // ❌ BAD: Memory allocation
    std::vector<float> temp(buffer.getNumSamples());

    // ❌ BAD: File I/O
    juce::File file("output.wav");

    // ❌ BAD: Locking (if avoidable)
    const juce::ScopedLock lock(mutex);

    // ❌ BAD: Logging/printing
    std::cout << "Processing" << std::endl;
    DBG("Debug message");

    // ❌ BAD: Calling non-real-time-safe functions
    loadPreset("default.xml");
}
```

**DO these instead**:
```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // ✅ GOOD: Pre-allocated buffer (member variable)
    tempBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

    // ✅ GOOD: Lock-free atomic reads
    float gainValue = gainParameter->load();

    // ✅ GOOD: Lock-free communication
    if (parameterChanged.exchange(false))
        updateInternalState();

    // ✅ GOOD: Efficient iteration
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            channelData[i] *= gainValue;
    }
}

// In class definition:
private:
    juce::AudioBuffer<float> tempBuffer;  // Pre-allocated
    std::atomic<float> gainParameter { 0.5f };
    std::atomic<bool> parameterChanged { false };
```

---

## Parameter Management

### Using AudioProcessorValueTreeState (Recommended)

```cpp
class MyAudioProcessor : public juce::AudioProcessor
{
public:
    MyAudioProcessor()
        : apvts(*this, nullptr, "Parameters", createParameters())
    {
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameters()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "gain",                           // parameterID
            "Gain",                           // parameter name
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.5f));                          // default value

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            "filterType",
            "Filter Type",
            juce::StringArray("Lowpass", "Highpass", "Bandpass"),
            0));                             // default index

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "bypass",
            "Bypass",
            false));                         // default value

        return { params.begin(), params.end() };
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        // Access parameter values
        auto gain = apvts.getRawParameterValue("gain")->load();
        auto bypass = apvts.getRawParameterValue("bypass")->load() > 0.5f;

        if (bypass)
            return;

        // Apply gain
        buffer.applyGain(gain);
    }

    void getStateInformation(juce::MemoryBlock& destData) override
    {
        // Save state
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }

    void setStateInformation(const void* data, int sizeInBytes) override
    {
        // Restore state
        std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
        if (xml && xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    juce::AudioProcessorValueTreeState apvts;
};
```

### Attaching Parameters to UI

```cpp
class MyEditor : public juce::AudioProcessorEditor
{
public:
    MyEditor(MyAudioProcessor& p)
        : AudioProcessorEditor(&p),
          gainSlider(),
          gainAttachment(p.apvts, "gain", gainSlider)
    {
        addAndMakeVisible(gainSlider);
        gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    }

private:
    juce::Slider gainSlider;
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
};
```

---

## DSP Processing Patterns

### 1. Using ProcessorChain

Efficient way to chain effects:

```cpp
using namespace juce::dsp;

// Define the chain
using EffectChain = ProcessorChain<
    Gain<float>,
    IIR::Filter<float>,
    WaveShaper<float>,
    Reverb
>;

class MyProcessor : public AudioProcessor
{
public:
    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = samplesPerBlock;
        spec.numChannels = 2;

        chain.prepare(spec);

        // Configure individual processors
        auto& gain = chain.get<0>();
        gain.setGainLinear(0.5f);

        auto& filter = chain.get<1>();
        *filter.state = *IIR::Coefficients<float>::makeLowPass(sampleRate, 1000.0f);
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        AudioBlock<float> block(buffer);
        ProcessContextReplacing<float> context(block);
        chain.process(context);
    }

private:
    EffectChain chain;
};
```

### 2. Oversampling

Reduce aliasing in non-linear processing:

```cpp
class OversampledProcessor : public AudioProcessor
{
public:
    OversampledProcessor()
        : oversampling(2,  // Number of channels
                      2,   // Oversampling factor (2x, 4x, etc.)
                      Oversampling<float>::filterHalfBandPolyphaseIIR)
    {
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        oversampling.initProcessing(samplesPerBlock);

        ProcessSpec spec;
        spec.sampleRate = sampleRate * oversampling.getOversamplingFactor();
        spec.maximumBlockSize = samplesPerBlock * oversampling.getOversamplingFactor();
        spec.numChannels = 2;

        distortion.prepare(spec);
    }

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        // Upsample
        AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling.processSamplesUp(block);

        // Process at higher sample rate
        ProcessContextReplacing<float> context(oversampledBlock);
        distortion.process(context);

        // Downsample
        oversampling.processSamplesDown(block);
    }

private:
    Oversampling<float> oversampling;
    WaveShaper<float> distortion;
};
```

### 3. SIMD Optimization

```cpp
void processBlockSIMD(AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);

        // Process in SIMD-sized chunks
        int i = 0;
        for (; i < numSamples - 4; i += 4)
        {
            // Load 4 samples
            auto samples = juce::dsp::SIMDRegister<float>::fromRawArray(data + i);

            // Process
            samples = samples * 0.5f;  // Apply gain

            // Store back
            samples.copyToRawArray(data + i);
        }

        // Process remaining samples
        for (; i < numSamples; ++i)
            data[i] *= 0.5f;
    }
}
```

---

## MIDI Processing

### Processing MIDI Messages

```cpp
void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
{
    // Iterate through MIDI messages
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        const int samplePosition = metadata.samplePosition;

        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            float velocity = msg.getVelocity() / 127.0f;
            synth.noteOn(note, velocity);
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            synth.noteOff(note);
        }
        else if (msg.isController())
        {
            int controller = msg.getControllerNumber();
            int value = msg.getControllerValue();
            handleCC(controller, value);
        }
    }

    // Clear MIDI if needed
    midiMessages.clear();

    // Add MIDI output
    midiMessages.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
}
```

### Synthesizer Pattern

```cpp
class MySynth : public juce::Synthesiser
{
public:
    MySynth()
    {
        // Add voices
        for (int i = 0; i < 8; ++i)
            addVoice(new MySynthVoice());

        // Add sounds
        addSound(new MySynthSound());
    }
};

class MySynthVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound*) override { return true; }

    void startNote(int midiNoteNumber, float velocity,
                  juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        level = velocity;
        phase = 0.0;
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (!allowTailOff)
            clearCurrentNote();
        // Otherwise, trigger release envelope
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        int startSample, int numSamples) override
    {
        if (!isVoiceActive())
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            // Generate oscillator
            float sample = std::sin(phase * juce::MathConstants<float>::twoPi);
            sample *= level;

            // Add to all channels
            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                outputBuffer.addSample(ch, startSample + i, sample);

            // Increment phase
            phase += frequency / getSampleRate();
            if (phase >= 1.0)
                phase -= 1.0;
        }
    }

private:
    double phase = 0.0;
    double frequency = 440.0;
    float level = 0.0f;
};
```

---

## Advanced Patterns

### Delay Line / Circular Buffer

```cpp
class DelayLine
{
public:
    void prepare(int maxDelaySamples)
    {
        buffer.resize(maxDelaySamples, 0.0f);
        writePos = 0;
    }

    void push(float sample)
    {
        buffer[writePos] = sample;
        writePos = (writePos + 1) % buffer.size();
    }

    float read(int delaySamples) const
    {
        int readPos = (writePos - delaySamples + buffer.size()) % buffer.size();
        return buffer[readPos];
    }

private:
    std::vector<float> buffer;
    int writePos = 0;
};
```

### Envelope Follower

```cpp
class EnvelopeFollower
{
public:
    void setSampleRate(double sr)
    {
        sampleRate = sr;
        setAttackTime(10.0);   // 10ms
        setReleaseTime(100.0); // 100ms
    }

    void setAttackTime(double ms)
    {
        attackCoeff = std::exp(-1.0 / (ms * 0.001 * sampleRate));
    }

    void setReleaseTime(double ms)
    {
        releaseCoeff = std::exp(-1.0 / (ms * 0.001 * sampleRate));
    }

    float process(float input)
    {
        float inputAbs = std::abs(input);

        if (inputAbs > envelope)
            envelope = attackCoeff * (envelope - inputAbs) + inputAbs;
        else
            envelope = releaseCoeff * (envelope - inputAbs) + inputAbs;

        return envelope;
    }

private:
    double sampleRate = 44100.0;
    double attackCoeff = 0.0;
    double releaseCoeff = 0.0;
    float envelope = 0.0f;
};
```

### FFT Processing

```cpp
class FFTProcessor
{
public:
    FFTProcessor()
        : fft(fftOrder),
          fftSize(1 << fftOrder)
    {
        fftData.resize(fftSize * 2);
    }

    void processFrame(const float* inputData)
    {
        // Copy to FFT buffer
        for (int i = 0; i < fftSize; ++i)
        {
            fftData[i] = inputData[i];
            fftData[fftSize + i] = 0.0f;  // Imaginary part
        }

        // Forward FFT
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        // Process in frequency domain
        for (int i = 0; i < fftSize / 2; ++i)
        {
            fftData[i] *= 0.5f;  // Example: attenuate
        }

        // Inverse FFT
        fft.performRealOnlyInverseTransform(fftData.data());

        // Copy output
        // ...
    }

private:
    static constexpr int fftOrder = 10;  // 2^10 = 1024 samples
    const int fftSize;
    juce::dsp::FFT fft;
    std::vector<float> fftData;
};
```

---

## Performance Tips

1. **Pre-allocate all buffers** in prepareToPlay()
2. **Use juce::dsp::AudioBlock** for efficient buffer operations
3. **Minimize branching** in processBlock() (use SIMD when possible)
4. **Use lookup tables** for expensive math (sin, exp, etc.)
5. **Profile with Instruments/VTune** to find bottlenecks
6. **Consider multicore** with juce::dsp::ProcessorChain (processes in parallel when possible)
7. **Avoid virtual function calls** in inner loops
8. **Use fixed-point math** when appropriate (mobile platforms)

---

## Testing Audio Code

```cpp
#include <juce_audio_processors/juce_audio_processors.h>

class AudioProcessorTest : public juce::UnitTest
{
public:
    AudioProcessorTest() : juce::UnitTest("AudioProcessor Tests") {}

    void runTest() override
    {
        beginTest("Silence in, silence out");

        MyAudioProcessor processor;
        processor.prepareToPlay(44100.0, 512);

        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        buffer.clear();

        processor.processBlock(buffer, midi);

        expect(buffer.getMagnitude(0, 512) == 0.0f);
    }
};

static AudioProcessorTest audioProcessorTest;
```
