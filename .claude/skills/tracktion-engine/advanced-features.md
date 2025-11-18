# Advanced Features

Advanced functionality including automation, MIDI, rendering, and recording.

## Automation System

### Parameter Automation

```cpp
// Get automatable parameter
auto plugin = track->pluginList[0];
auto param = plugin->getAutomatableParameter(paramIndex);

// Enable automation
param->setAutomationActive(true);

// Access automation curve
auto& curve = param->getCurve();

// Add automation points
curve.addPoint(timeInSeconds, value, curvature);
curve.setPosition(timeInSeconds, newValue);
curve.removePoint(pointIndex);

// Query automation
auto value = param->getCurveValue(timeInSeconds);

// MIDI learn
param->midiLearnMode = true;
```

### Parameter Modifiers

Modifiers add dynamic modulation to parameters:

#### LFO Modifier

```cpp
auto lfo = new te::LFOModifier(*param);
lfo->rate = 2.0;   // 2 Hz
lfo->depth = 0.5;  // 50% modulation depth
lfo->shape = te::LFOModifier::Shape::sine;
lfo->phase = 0.0;
lfo->offset = 0.0;
```

#### Envelope Follower

```cpp
auto envFollower = new te::EnvelopeFollowerModifier(*param);
envFollower->attack = 0.01;   // 10ms
envFollower->release = 0.5;    // 500ms
envFollower->depth = 1.0;
envFollower->enabled = true;
```

#### Breakpoint Modifier

```cpp
auto breakpoint = new te::BreakpointOscillatorModifier(*param);
breakpoint->addPoint(0.0, 0.0, 0.5);
breakpoint->addPoint(0.5, 1.0, 0.5);
breakpoint->addPoint(1.0, 0.0, 0.5);
breakpoint->syncType = te::BreakpointOscillatorModifier::bar;
```

#### Step Modifier

```cpp
auto step = new te::StepModifier(*param);
step->setNumSteps(16);
step->setValue(stepIndex, value);
step->rate = te::StepModifier::Rate::sixteenth;
```

#### Random Modifier

```cpp
auto random = new te::RandomModifier(*param);
random->rate = 4.0;     // Random value every 4 beats
random->depth = 0.3;    // 30% randomization
random->bipolar = true; // -depth to +depth
```

#### MIDI Tracker

```cpp
auto midiTracker = new te::MIDITrackerModifier(*param);
midiTracker->source = te::MIDITrackerModifier::Source::noteVelocity;
// Other sources: aftertouch, pitchBend, CC
```

### Macro Parameters

```cpp
// Get macro parameter list
auto& macroList = edit->getMacroParameterList();

// Create macro parameter
auto macro = macroList.createMacroParameter();
macro->setParameterName("Cutoff");
macro->setParameterDescription("Filter cutoff control");

// Assign parameters to macro
macro->addParameter(plugin1->getAutomatableParameter(0), 0.0, 1.0);
macro->addParameter(plugin2->getAutomatableParameter(2), 0.5, 1.0);
// min/max define the range of control

// Control macro (controls all assigned parameters)
macro->setNormalisedParameter(0.75);

// Remove assignment
macro->removeParameter(parameter);
```

## Advanced MIDI Features

### MIDI Pattern Generation

```cpp
// Generate bass pattern
auto bassPattern = te::generateBassPattern(
    edit, scale, rootNote, numBars, complexity);

// Generate melody
auto melody = te::generateMelodyPattern(
    edit, scale, rootNote, numBars, complexity);

// Generate chord progression
auto chords = te::generateChordPattern(
    edit, scale, rootNote, numBars, complexity);

// Generate arpeggio
auto arp = te::generateArpPattern(
    edit, scale, rootNote, numBars, arpType);
```

### Quantization and Groove

```cpp
auto& midiList = midiClip->getSequence();

// Quantization
te::Quantisation quant;
quant.setType(te::Quantisation::Type::sixteenth);
quant.setProportion(0.75);  // 75% quantization
midiList.quantise(quant, nullptr);

// Groove templates
auto groove = te::GrooveTemplate::createGroove(engine, grooveFile);
midiList.applyGrooveTemplate(*groove);
```

### MPE (MIDI Polyphonic Expression)

```cpp
// Enable MPE for clip
midiClip->setMPEMode(true);

// Per-note expression
auto note = midiList.getNote(index);
note->setPitchBend(pitchBendValue);  // Per-note pitch bend
note->setPressure(pressureValue);     // Per-note aftertouch
note->setTimbre(timbreValue);         // Per-note timbre (CC74)

// Export to MPE MIDI file
edit->exportToMPEMIDI(outputFile);
```

### MIDI Recording

```cpp
// Enable MIDI recording on track
midiTrack->setRecordingActive(true, midiInputDevice);

// Start recording
transport.record(false);

// Stop recording
transport.stop(false, false);

// Get recorded clip
auto newClip = midiTrack->getClips().getLast();
```

## Clip Launching

### Clip Slots

```cpp
// Get clip slots for track
auto clipSlots = track->getClipSlotList();

// Create clip slot
auto slot = clipSlots->insertNewClip(slotIndex);

// Configure clip slot
slot->setClip(clip);
slot->setLength(4.0);  // 4 beats

// Launch control
slot->launch();
slot->stop();
slot->trigger();

// Follow actions
slot->setFollowAction(te::ClipSlot::FollowAction::playNext);
slot->setFollowActionTime(4.0);  // After 4 beats

// Query state
bool isPlaying = slot->isPlaying();
bool isQueued = slot->isQueued();
```

### Scenes

```cpp
// Get scene
auto scene = edit->getClipSlotList().getScene(sceneIndex);

// Launch scene (launches all clips in scene)
scene->launch();
scene->stop();

// Add/remove clips from scene
scene->addClip(clipSlot);
scene->removeClip(clipSlot);
```

## Rendering and Export

### Basic Rendering

```cpp
// Create renderer parameters
te::Renderer::Parameters params(*edit);
params.destFile = outputFile;

// Audio format
auto& formatMgr = engine.getAudioFileFormatManager();
params.audioFormat = formatMgr.getNamedFormat("WAV");
params.sampleRate = 44100.0;
params.bitDepth = 16;

// Time range
params.time = { startTime, endTime };

// Render specific tracks
params.tracksToDo.add(track);

// Quality settings
params.quality = te::Renderer::Parameters::Quality::high;
params.realTime = false;  // Faster than real-time

// Perform render
te::Renderer renderer(params);
while (renderer.isRendering())
{
    auto progress = renderer.getProgress();
    // Update UI progress bar
    juce::Thread::sleep(100);
}

bool success = renderer.getResult();
```

### Advanced Render Options

```cpp
// Normalization
params.normalise = true;
params.normaliseToLevel = 0.0;  // dB

// Silence trimming
params.trimSilence = true;
params.trimSilenceThresholdDb = -50.0;

// Create MIDI file
params.createMidiFile = true;

// Render selection only
params.usePlugins = true;
params.addAntiDenormalisationNoise = false;

// Multi-track rendering
params.separateFilesForMultipleChannels = true;

// Background rendering
renderer.renderInBackground();
```

### Render Listeners

```cpp
struct RendererListener : te::Renderer::Listener
{
    void renderProgress(float progress) override
    {
        // progress: 0.0 to 1.0
        std::cout << "Progress: " << (progress * 100) << "%\n";
    }

    void renderComplete() override
    {
        std::cout << "Render complete!\n";
    }
};

renderer.addListener(myListener);
```

## Audio Recording

### Recording Setup

```cpp
auto& transport = edit->getTransport();
auto& deviceMgr = engine.getDeviceManager();
auto inputDevice = deviceMgr.getDefaultWaveInDevice();

// Enable recording for track
track->setRecordingActive(true, inputDevice);

// Set punch in/out
transport.setLoopRange({ punchInTime, punchOutTime });
transport.looping = true;
```

### Recording Control

```cpp
// Start recording
transport.record(false);  // false = don't auto-return

// Punch recording (with loop range set)
transport.record(true);   // true = auto-return after punch

// Stop recording
transport.stop(false, false);  // Don't discard recordings

// Get recorded clip
auto recordedClip = track->getClips().getLast();
```

### Recording Monitoring

```cpp
// Input monitoring
track->setInputMonitoringEnabled(true);

// Recording meters
auto inputLevel = inputDevice->getInputLevel();
```

## Track Freezing

### Freeze Track

```cpp
// Insert freeze point
auto freezePoint = track->pluginList.insertPlugin(
    te::Plugin::create("freezePoint"), index);

// Freeze track up to freeze point
track->freezeTrackUpTo(freezePoint,
                       groupFreeze,   // Freeze sub-tracks too
                       allowAsync);   // Allow background freeze

// Check freeze state
bool isFrozen = track->isFrozen();
bool isFreezing = track->isFreezing();
```

### Unfreeze Track

```cpp
// Unfreeze
track->unFreezeTrack();

// Individual freeze clips
auto freezeClip = track->getFreezeClip();
```

## Click Track

### Click Configuration

```cpp
auto& clickTrack = edit->getClickTrack();

// Enable/disable
clickTrack.setEnabled(true);

// Custom click samples
clickTrack.setAccentedBeatFile(accentFile);
clickTrack.setUnaccentedBeatFile(beatFile);

// Click properties
clickTrack.setLevel(0.75f);
clickTrack.setOnlyDuringRecording(true);
clickTrack.setOnlyDuringCountIn(false);

// MIDI click
clickTrack.setMidiClickNote(60);  // Middle C
clickTrack.setMidiClickDevice(midiOutDevice);
```

## Time Stretching

### Configure Time Stretch

```cpp
// Set time stretch engine
#define TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH 1
#define TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE 1
#define TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND 1

// Set algorithm per clip
audioClip->setTimeStretchMode(te::TimeStretcher::elastiqueTransient);

// Other modes:
// - elastiquePro
// - elastiqueTransient
// - elastiqueMobile
// - soundTouchNormal
// - soundTouchBetter
// - soundTouchBest
// - rubberBandMelodic
// - rubberBandPercussive

// Speed and pitch
audioClip->setSpeedRatio(1.5);       // 1.5x speed
audioClip->setPitchChange(2.0);      // +2 semitones
audioClip->setAutoTempo(true);       // Follow edit tempo
audioClip->setAutoPitch(true);       // Follow edit key
```

## Ableton Link

### Link Synchronization

```cpp
#define TRACKTION_ENABLE_ABLETON_LINK 1

// Enable Link
transport.enableAbletonLink(true);

// Link settings
transport.setAbletonLinkStart(enabled);

// Query Link state
auto numPeers = transport.getNumAbletonLinkPeers();
bool isLinkEnabled = transport.isAbletonLinkEnabled();
```

## Control Surfaces

### External Controllers

```cpp
// Get available control surfaces
auto& controllerMgr = engine.getExternalControllerManager();
auto devices = controllerMgr.getControllerDevices();

// Create controller
auto mcu = controllerMgr.createControllerDevice(
    te::MackieControlUnit::deviceType);

// Enable controller
mcu->enable();
mcu->disable();

// Built-in support for:
// - Mackie Control Universal (MCU)
// - Mackie C4
// - Alpha Track
// - Novation Automap
// - Novation Remote SL Compact
// - Tranzport
```

### Custom Control Surface

```cpp
class MyControlSurface : public te::ExternalController
{
public:
    void moveFaders(float* positions, int numFaders) override
    {
        // Update hardware faders
    }

    void movePanPots(float* positions, int numPots) override
    {
        // Update hardware pan controls
    }

    void updateSoloAndMute() override
    {
        // Update solo/mute LEDs
    }

    void updateDisplay() override
    {
        // Update LCD display
    }

    // ... implement other methods
};
```

## ReWire Support

```cpp
#define TRACKTION_ENABLE_REWIRE 1

// Get ReWire devices
auto rewireDevices = te::ReWirePlugin::getAvailableDevices();

// Create ReWire plugin
for (auto& device : rewireDevices)
{
    auto plugin = track->pluginList.insertPlugin(
        te::ReWirePlugin::create(device), index);
}
```

## ARA (Audio Random Access)

```cpp
#define TRACKTION_ENABLE_ARA 1

// ARA plugins automatically integrate with Edit structure
// Provides deep host/plugin integration

// Check if plugin supports ARA
bool supportsARA = plugin->supportsARA();

// ARA plugins can:
// - Access full Edit structure
// - Provide advanced audio analysis
// - Offer non-destructive editing
```
