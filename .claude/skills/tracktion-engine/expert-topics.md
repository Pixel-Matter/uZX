# Expert Topics

Advanced optimization, threading, custom plugin development, and low-level concepts.

## Threading Model

### Thread Types

**Message Thread**: UI updates, Edit modifications, non-real-time operations
- Safe to modify Edit state
- Can call any Tracktion Engine API
- Use for user interactions

**Audio Thread**: Real-time audio processing (callback)
- Never block or allocate memory
- Only lock-free operations
- Minimal computation
- Use pre-allocated buffers

**Recording Thread**: Disk I/O for recording
- Handles writing audio to disk
- Communicates with audio thread via lock-free FIFO

**Background Threads**: Asset generation, rendering, plugin scanning
- Thumbnail generation
- Audio file analysis
- Plugin discovery

### Thread Safety Rules

```cpp
// ✓ SAFE: Modify Edit on message thread
juce::MessageManager::callAsync([&]()
{
    track->insertNewClip(/*...*/);
});

// ✗ UNSAFE: Modify Edit on audio thread
void processBlock(juce::AudioBuffer<float>& buffer)
{
    // DON'T DO THIS!
    // track->insertNewClip(/*...*/);
}

// ✓ SAFE: Lock-free communication
juce::AbstractFifo fifo(bufferSize);

// Audio thread writes
fifo.prepareToWrite(numToWrite).forEach([&](int idx) {
    buffer[idx] = audioData[idx];
});

// Message thread reads
fifo.prepareToRead(numReady).forEach([&](int idx) {
    processData(buffer[idx]);
});

// ✓ SAFE: Query from audio thread using callBlocking
auto value = edit->callBlocking([&]() { return someValue; });
```

### Multi-CPU Processing

```cpp
// Configure thread pool
auto& editPlaybackContext = edit->getCurrentPlaybackContext();
editPlaybackContext->setNumThreads(numCPUs);

// Choose threading algorithm
using PlayHeadType = tracktion::graph::PlayHeadState;

// Single-threaded (for debugging)
PlayHeadType::PlayHeadPositionUpdaterType::singleThreaded;

// Multi-threaded (production)
PlayHeadType::PlayHeadPositionUpdaterType::multiThreaded;
```

## Audio Graph Architecture

### Node-Based Processing

The `tracktion_graph` module provides node-based audio processing:

```cpp
// Custom node
class MyNode : public tracktion::graph::Node
{
public:
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        return {
            numberOfChannels,
            latencyNumSamples,
            hasMidi,
            hasAudio
        };
    }

    void prepareToPlay(const PlaybackInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        blockSize = info.blockSize;
        // Allocate buffers, setup DSP
    }

    bool isReadyToProcess() override
    {
        return true;
    }

    void process(ProcessContext& pc) override
    {
        auto inputBuffers = pc.buffers;
        auto outputBuffers = pc.buffers;
        auto numSamples = pc.numSamples;

        // Process audio
        for (int ch = 0; ch < outputBuffers.audio.getNumChannels(); ++ch)
        {
            auto* samples = outputBuffers.audio.getChannel(ch);
            // ... DSP processing ...
        }

        // Process MIDI
        if (! pc.buffers.midi.isEmpty())
        {
            for (auto& msg : pc.buffers.midi)
            {
                // ... process MIDI ...
            }
        }
    }
};

// Build graph
std::vector<std::unique_ptr<Node>> nodes;
nodes.push_back(std::make_unique<MyNode>());
auto graph = tracktion::graph::createNode(std::move(nodes));

// Play graph
tracktion::graph::PlayHead playHead;
tracktion::graph::PlayHeadState playHeadState(playHead);
```

### Node Optimization

```cpp
// Report latency for compensation
int getLatencySamples() override
{
    return latencyInSamples;
}

// Tail length (for reverbs, delays)
std::optional<double> getTailLength() override
{
    return tailLengthSeconds;
}

// Efficient channel mapping
void mapChannels(const std::vector<int>& channelMap)
{
    // Handle non-contiguous channel routing
}
```

## Custom Plugin Development

### Plugin Base Class

```cpp
class MyCustomPlugin : public te::Plugin
{
public:
    MyCustomPlugin(te::PluginCreationInfo info)
        : Plugin(info)
    {
        // Initialize parameters
        auto um = getUndoManager();

        gainParam = addParam("gain", "Gain",
                            { -24.0f, 24.0f },
                            [] (float v) { return juce::String(v, 1) + " dB"; },
                            [] (const juce::String& s) { return s.getFloatValue(); });
        gainParam->attachToCurrentValue(gain);
    }

    static const char* xmlTypeName;
    static const char* getPluginName() { return "My Plugin"; }

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }

    void initialise(const PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        // Setup DSP
    }

    void deinitialise() override
    {
        // Cleanup
    }

    void applyToBuffer(const PluginRenderContext& prc) override
    {
        auto& destBuffer = prc.destBuffer;

        if (destBuffer.getNumChannels() == 0)
            return;

        // Apply gain
        auto gainValue = juce::Decibels::decibelsToGain(gain);
        destBuffer.applyGain(gainValue);
    }

    juce::ReferenceCountedArray<AutomatableParameter> getAutomatableParameters() override
    {
        return { gainParam };
    }

    void restorePluginStateFromValueTree(const juce::ValueTree& v) override
    {
        juce::CachedValue<float>* cvsFloat[] = { &gain, nullptr };
        copyPropertiesToCachedValues(v, cvsFloat);
    }

private:
    double sampleRate = 44100.0;
    AutomatableParameter::Ptr gainParam;
    juce::CachedValue<float> gain { 0.0f };
};

// Register plugin type
const char* MyCustomPlugin::xmlTypeName = "myCustomPlugin";

// In your initialization code:
te::Plugin::registerPluginType<MyCustomPlugin>();
```

### Plugin State Persistence

```cpp
void restorePluginStateFromValueTree(const juce::ValueTree& v) override
{
    // Cached values (automatically persisted)
    juce::CachedValue<float>* cvsFloat[] = { &param1, &param2, nullptr };
    juce::CachedValue<bool>* cvsBool[] = { &enabled, nullptr };

    copyPropertiesToCachedValues(v, cvsFloat);
    copyPropertiesToCachedValues(v, cvsBool);
}

// State is automatically saved to ValueTree
```

## Performance Optimization

### Audio File Caching

```cpp
// Configure cache size
auto& audioFileMgr = engine.getAudioFileManager();
audioFileMgr.setCacheSize(256 * 1024 * 1024);  // 256MB

// Proxy files for faster loading
audioFile.createProxyIfNeeded();

// Pre-load audio files
audioFileMgr.warmUpCache(audioFile);
```

### Memory Management

```cpp
// Use scratch buffers (pre-allocated)
auto scratch = engine.getAudioFileManager()
    .getScratchBuffer(numChannels, numSamples);

// Use rpmalloc for better performance
#define TRACKTION_ENABLE_RPMALLOC 1

// Avoid allocations in audio thread
void process(ProcessContext& pc)
{
    // ✓ GOOD: Pre-allocated buffer
    workBuffer.setSize(pc.buffers.audio.getNumChannels(),
                       pc.numSamples,
                       false, false, true);

    // ✗ BAD: Allocation in audio thread
    // std::vector<float> temp(pc.numSamples);
}
```

### Plugin Latency Compensation

```cpp
// Report plugin latency
int getLatencySamples() override
{
    return processingLatency;
}

// Engine automatically compensates across all tracks
```

### Optimization Flags

```cpp
// Build optimizations
#define TRACKTION_ENABLE_RPMALLOC 1      // Fast allocator
#define TRACKTION_LOG_ENABLED 0          // Disable logging
#define JUCE_USE_VDSP_FRAMEWORK 1        // Use Accelerate (macOS)

// CPU-specific optimizations
#if JUCE_INTEL
    #define JUCE_USE_SSE_INTRINSICS 1
#elif JUCE_ARM
    #define JUCE_USE_ARM_NEON 1
#endif
```

## ValueTree State Management

### Understanding ValueTree

All Tracktion Engine objects use JUCE's `ValueTree` for state:

```cpp
// Access underlying state
auto& state = edit->state;
auto& trackState = track->state;
auto& clipState = clip->state;

// ValueTree properties
trackState.setProperty(te::IDs::name, "My Track", &undoManager);
auto trackName = trackState[te::IDs::name].toString();

// Listen to changes
struct StateListener : juce::ValueTree::Listener
{
    void valueTreePropertyChanged(juce::ValueTree& tree,
                                  const juce::Identifier& prop) override
    {
        if (prop == te::IDs::name)
            updateUI();
    }

    void valueTreeChildAdded(juce::ValueTree& parent,
                            juce::ValueTree& child) override {}

    void valueTreeChildRemoved(juce::ValueTree& parent,
                              juce::ValueTree& child,
                              int index) override {}
};

trackState.addListener(myListener);
```

### Direct State Manipulation

```cpp
// Advanced: Direct state modification
track->state.setProperty(te::IDs::colour,
                        juce::Colour(0xff0000ff).toString(),
                        &edit->getUndoManager());

// Batch modifications
{
    juce::ValueTree::ScopedEventSuppressor suppressor(trackState);

    trackState.setProperty(te::IDs::name, "Track 1", nullptr);
    trackState.setProperty(te::IDs::mute, true, nullptr);
    trackState.setProperty(te::IDs::solo, false, nullptr);

    // All changes notified at once when suppressor destroyed
}
```

## Engine Customization

### UIBehaviour

```cpp
struct MyUIBehaviour : te::UIBehaviour
{
    bool shouldGenerateLiveWaveformsWhenRecording() override
    {
        return true;
    }

    int getTimecodeFormat() override
    {
        return te::TimecodeDisplayFormat::barsBeats;
    }

    double getDefaultLoopLength() override
    {
        return 8.0;  // 8 seconds
    }

    juce::File getDefaultLoadSaveDirectory(const juce::String& type) override
    {
        return juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory);
    }

    // ... override other methods as needed
};

engine.setUIBehaviour(std::make_unique<MyUIBehaviour>());
```

### EngineBehaviour

```cpp
struct MyEngineBehaviour : te::EngineBehaviour
{
    bool autoInitialiseDevices() override
    {
        return false;  // Manual device init
    }

    double getDefaultLoopLength() override
    {
        return 8.0;
    }

    int getNumberOfCPUsToUseForAudio() override
    {
        return juce::SystemStats::getNumCpus() - 1;
    }

    juce::File getApplicationCacheFolder() override
    {
        return juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("MyApp");
    }
};

engine.setEngineBehaviour(std::make_unique<MyEngineBehaviour>());
```

### PropertyStorage

```cpp
// Custom settings storage
struct MyPropertyStorage : te::PropertyStorage
{
    juce::var getProperty(const juce::String& name,
                         const juce::var& defaultValue) override
    {
        // Load from custom storage
        return settings.getValue(name, defaultValue);
    }

    void setProperty(const juce::String& name,
                    const juce::var& value) override
    {
        // Save to custom storage
        settings.setValue(name, value);
        settings.saveIfNeeded();
    }

private:
    juce::PropertiesFile settings;
};
```

## Debugging and Profiling

### Crash Tracing

```cpp
// Enable crash tracing
te::CrashTracer::Ptr tracer(new te::CrashTracer("MyFunction"));

// Scoped tracing
void myFunction()
{
    CRASH_TRACER;  // Automatic tracer for this scope

    // ... code that might crash ...
}
```

### Benchmarking

```cpp
#define TRACKTION_BENCHMARKS 1

// Benchmark code
TRACKTION_BENCHMARK("MyOperation")
{
    // Code to benchmark
    performExpensiveOperation();
}

// Results available at:
// https://tracktion.github.io/tracktion_engine/benchmarks.html
```

### Unit Testing

```cpp
#define TRACKTION_UNIT_TESTS 1

// Unit tests use doctest framework
TEST_CASE("My test")
{
    te::Engine engine { "Test" };
    auto edit = te::createEmptyEdit(engine);

    REQUIRE(edit != nullptr);
    CHECK(edit->getAllTracks().size() == 0);
}
```

### Logging

```cpp
// Enable logging
#define TRACKTION_LOG_ENABLED 1

// Log messages
TRACKTION_LOG("Message");
TRACKTION_LOG_ERROR("Error message");

// Custom log handler
struct MyLogger : te::Logger
{
    void logMessage(const juce::String& message) override
    {
        std::cout << message << "\n";
    }
};

te::Logger::setLogger(std::make_unique<MyLogger>());
```

## Advanced Audio Processing

### Custom Time Stretching

```cpp
// Implement custom time stretch algorithm
class MyTimeStretcher : public te::TimeStretcher
{
public:
    bool isOk() override { return true; }

    bool initialise(double sourceSampleRate,
                   int samplesPerBlock,
                   int numChannels,
                   bool realtime) override
    {
        // Setup algorithm
        return true;
    }

    void reset() override
    {
        // Reset state
    }

    int getMaxFramesNeeded(int numFrames) override
    {
        return numFrames * 2;  // Conservative estimate
    }

    int processData(const float* const* inChannels,
                   int numSamples,
                   float* const* outChannels) override
    {
        // Process audio with time stretch
        return numSamplesGenerated;
    }
};
```

### DSP Utilities

```cpp
// ADSR envelope
te::Envelope envelope;
envelope.attackTime = 0.01;
envelope.decayTime = 0.1;
envelope.sustainLevel = 0.7;
envelope.releaseTime = 0.2;

auto value = envelope.process(gateOn);

// Band-limited oscillators
te::BandLimitedLookupTable lookupTable;
lookupTable.initialise(sampleRate);

float sawSample = lookupTable.getSaw(phase);
float squareSample = lookupTable.getSquare(phase);
float triangleSample = lookupTable.getTriangle(phase);

// Audio fade curves
auto fadeCurve = te::AudioFadeCurve::linear;
// Other curves: sCurve, convex, concave

auto gain = te::getGainForCurve(fadeCurve, position);
```

## Compiling with Tracktion Engine

### CMake Configuration

```cmake
# Find JUCE
find_package(JUCE CONFIG REQUIRED)

# Add Tracktion Engine modules
juce_add_module(path/to/tracktion_engine)
juce_add_module(path/to/tracktion_graph)
juce_add_module(path/to/tracktion_core)

# Your target
juce_add_gui_app(MyApp
    PRODUCT_NAME "My App")

target_sources(MyApp PRIVATE main.cpp)

target_link_libraries(MyApp PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    tracktion::tracktion_engine)

# C++20 required
target_compile_features(MyApp PRIVATE cxx_std_20)
```

### Compile Flags

```cpp
// Feature flags (set in CMakeLists.txt or preprocessor)
TRACKTION_ENABLE_ARA=1
TRACKTION_ENABLE_CMAJOR=1
TRACKTION_ENABLE_REWIRE=1
TRACKTION_ENABLE_AUTOMAP=1
TRACKTION_ENABLE_VIDEO=1
TRACKTION_ENABLE_REX=1
TRACKTION_ENABLE_CONTROL_SURFACES=1
TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE=1
TRACKTION_ENABLE_ABLETON_LINK=1
TRACKTION_AIR_WINDOWS=1
TRACKTION_UNIT_TESTS=1
TRACKTION_BENCHMARKS=1
```

## Platform-Specific Considerations

### iOS/Android

```cpp
// Reduce memory usage
#define TRACKTION_ENABLE_RPMALLOC 0  // Use system allocator

// Disable expensive features
#define TRACKTION_ENABLE_VIDEO 0
#define TRACKTION_AIR_WINDOWS 0

// Use efficient time stretch
#define TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH 1
#define TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE 0  // Requires license
```

### Raspberry Pi

```cpp
// Optimize for ARM
#define JUCE_USE_ARM_NEON 1

// Reduce CPU usage
editPlaybackContext->setNumThreads(2);  // Limited cores

// Disable heavy features
#define TRACKTION_ENABLE_VIDEO 0
```
