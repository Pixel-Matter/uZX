# API Reference

Detailed API reference for Tracktion Engine classes and methods.

## Track Types

### Creating Tracks

```cpp
// Audio track - plays audio clips
auto audioTrack = te::createTrack<te::AudioTrack>(*edit);

// MIDI track - plays MIDI clips
auto midiTrack = te::createTrack<te::MidiTrack>(*edit);

// Folder track - groups other tracks
auto folder = te::createTrack<te::FolderTrack>(*edit);

// Special tracks (usually auto-created)
te::TempoTrack      // Tempo automation
te::MarkerTrack     // Markers/cues
te::ChordTrack      // Chord progressions
te::ArrangerTrack   // Arrangement sections
te::AutomationTrack // Separate automation lane
```

### Track Properties and Methods

```cpp
// Track properties
track->getName();
track->setName("My Track");
track->getColour();
track->setColour(juce::Colours::blue);

// Track state
track->isMuted(false);
track->isSolo(false);
track->setMute(true);
track->setSolo(true);

// Volume and pan
track->getVolumePlugin()->setVolumeDb(6.0);
track->getVolumePlugin()->setPan(0.5);  // -1 to 1

// Get clips on track
auto clips = track->getClips();
for (auto clip : clips)
    processClip(clip);

// Plugin list
auto& pluginList = track->pluginList;
```

## Clip Types

### WaveAudioClip (Audio Clips)

```cpp
// Insert audio clip
auto clip = audioTrack->insertWaveClip("My Clip", audioFile,
                                      { {startTime, endTime}, offset });

// Clip timing
clip->setStart(newStart, true, true);
clip->setEnd(newEnd, true);
clip->getPosition();  // Returns EditTimeRange

// Audio properties
clip->setGainDB(6.0);
clip->setSpeedRatio(1.5);     // Time stretch (1.5x speed)
clip->setPitchChange(2.0);    // Pitch shift (+2 semitones)
clip->setAutoTempo(true);     // Follow edit tempo
clip->setAutoPitch(true);     // Follow edit key

// Fades
clip->setFadeIn(1.0);   // 1 second fade in
clip->setFadeOut(1.0);  // 1 second fade out
clip->setFadeInType(te::AudioFadeCurve::linear);
clip->setFadeOutType(te::AudioFadeCurve::sCurve);

// Loop properties
clip->setLoopStart(loopStartOffset);
clip->setLoopLength(loopLength);
clip->setLoopInfo({ loopStart, loopEnd, oneShot });

// Warp time
clip->setTimeStretchMode(te::TimeStretcher::elastiqueTransient);
```

### MidiClip (MIDI Clips)

```cpp
// Insert MIDI clip
auto midiClip = dynamic_cast<te::MidiClip*>(
    midiTrack->insertNewClip(te::TrackItem::Type::midi,
                            { startTime, duration }, nullptr));

// Get MIDI sequence
auto& sequence = midiClip->getSequence();

// Add notes
sequence.addNote(noteNumber, startBeat, lengthBeats,
                 velocity, colourIndex, nullptr);

// Remove notes
sequence.removeNote(note, nullptr);

// Get notes in range
auto notes = sequence.getNotesInRange({ startBeat, endBeat });
for (auto note : notes)
{
    note->setNoteNumber(newNote, nullptr);
    note->setVelocity(newVel, nullptr);
    note->setBeatPosition(newStart, nullptr);
    note->setLengthInBeats(newLength, nullptr);
}

// Controllers (CC)
sequence.addControllerEvent(beatNumber, controllerType,
                           value, metadata, nullptr);

// Quantization
te::Quantisation quant;
quant.setType(te::Quantisation::Type::sixteenth);
sequence.quantise(quant, nullptr);

// Groove templates
auto groove = te::GrooveTemplate::createGroove(engine, grooveFile);
sequence.applyGrooveTemplate(*groove);

// MPE (MIDI Polyphonic Expression)
midiClip->setMPEMode(true);
note->setPitchBend(pitchBendValue);
note->setPressure(pressureValue);
note->setTimbre(timbreValue);
```

### StepClip (Step Sequencer)

```cpp
auto stepClip = dynamic_cast<te::StepClip*>(clip);

// Get/create pattern
auto& pattern = stepClip->getPattern(channelIndex);

// Configure pattern
pattern.setNumNotes(16);  // 16 steps
pattern.setNoteValue(stepIndex, te::Pattern::Note, noteNumber);
pattern.setNoteValue(stepIndex, te::Pattern::Gate, gateLength);
pattern.setNoteValue(stepIndex, te::Pattern::Velocity, velocity);
pattern.setNoteValue(stepIndex, te::Pattern::Probability, probability);

// Pattern playback
pattern.setRateType(te::Pattern::Rate::sixteenth);
```

## Edit Structure

### Creating and Loading Edits

```cpp
// Create new empty edit
auto edit = te::Edit::createEmpty(engine,
    te::createEmptyEdit::Options{});

// Load existing edit
auto edit = te::loadEditFromFile(engine, editFile);

// Save edit
edit->saveAs(newFile, saveOptions);
edit->flushState();  // Save without changing file

// Edit info
auto file = edit->getEditFileReturnedByProjectManager();
auto name = edit->getProjectItemID().toString();
```

### Edit Properties

```cpp
// Time
edit->getLength();
edit->getMarkIn();
edit->getMarkOut();

// Subsystems
edit->getTransport();
edit->getTempoSequence();
edit->getPitchSequence();
edit->getMarkerManager();
edit->getUndoManager();
edit->getTransportControl();

// Tracks
auto tracks = edit->getAllTracks(te::Edit::TrackType::all);
auto audioTracks = edit->getAudioTracks();
edit->getTrackByName("Track Name");
edit->moveTrack(track, newIndex);
edit->deleteTrack(track);

// Master plugins
edit->getMasterVolumePlugin();
edit->getMasterPluginList();
```

## Plugin Management

### Plugin Discovery

```cpp
auto& pluginMgr = engine.getPluginManager();

// Scan for plugins
pluginMgr.scanForNewPlugins();

// Get available plugins
auto pluginList = pluginMgr.getKnownPlugins();

// Find specific plugin
auto desc = pluginMgr.getPluginDescriptionForIdentifierString(pluginID);
```

### Inserting Plugins

```cpp
// Insert plugin on track
auto plugin = track->pluginList.insertPlugin(
    te::Plugin::create(pluginDescription), index);

// Insert built-in plugin by type
auto eq = track->pluginList.insertPlugin(te::EqualizerPlugin::create(), 0);
auto compressor = track->pluginList.insertPlugin(te::CompressorPlugin::create(), 1);

// Remove plugin
track->pluginList.removePlugin(plugin);
```

### Plugin Parameters

```cpp
// Get automatable parameters
auto params = plugin->getAutomatableParameters();
for (auto param : params)
{
    auto name = param->getParameterName();
    auto value = param->getCurrentValue();
    param->setParameter(0.5f, juce::sendNotification);
}

// Get specific parameter
if (auto param = plugin->getAutomatableParameter(index))
{
    param->setNormalisedParameter(0.75);  // 0-1 range
    auto curveValue = param->getCurveValue(timeInSeconds);
}

// Automation
param->setAutomationActive(true);
auto& curve = param->getCurve();
curve.addPoint(timeInSeconds, value, curvature);

// MIDI learn
param->midiLearnMode = true;
```

### Built-in Plugins

```cpp
// Volume and Pan
auto volPan = dynamic_cast<te::VolumeAndPanPlugin*>(plugin);
volPan->setVolumeDb(6.0);
volPan->setPan(0.5);

// EQ (4-band)
auto eq = dynamic_cast<te::EqualizerPlugin*>(plugin);
eq->setBandGain(bandIndex, gainDb);
eq->setBandFrequency(bandIndex, frequency);
eq->setBandQ(bandIndex, qValue);

// Compressor
auto comp = dynamic_cast<te::CompressorPlugin*>(plugin);
comp->setThresholdDb(threshold);
comp->setRatio(ratio);
comp->setAttackMs(attack);
comp->setReleaseMs(release);

// Reverb
auto reverb = dynamic_cast<te::ReverbPlugin*>(plugin);
reverb->setRoomSize(size);
reverb->setDamping(damping);
reverb->setWetLevel(wetLevel);
reverb->setDryLevel(dryLevel);
```

## Audio File Handling

### AudioFile Class

```cpp
// Create audio file reference
te::AudioFile audioFile(engine, juce::File("path/to/audio.wav"));

// Properties
auto lengthInSamples = audioFile.getLength();
auto sampleRate = audioFile.getSampleRate();
auto numChannels = audioFile.getNumChannels();
auto lengthInSeconds = audioFile.getLengthInSeconds();

// File info
te::AudioFileInfo info;
audioFile.getInfo(info);

// Create reader
auto reader = audioFile.createReader();
if (reader)
{
    reader->read(destBuffer, startSample, numSamples,
                 relativeTo, clearExisting);
}

// Proxy files (for faster loading)
audioFile.createProxyIfNeeded();
audioFile.hasFile();
audioFile.isValid();
```

### AudioFileManager

```cpp
auto& audioFileMgr = engine.getAudioFileManager();

// Cache configuration
audioFileMgr.setCacheSize(256 * 1024 * 1024);  // 256MB

// Get/release audio files
auto audioFile = audioFileMgr.getAudioFile(file);

// Thumbnails
audioFileMgr.getOrCreateThumbnail(audioFile);
```

## Transport Control

### Playback Control

```cpp
auto& transport = edit->getTransport();

// Basic control
transport.play(autoReturnIfError);
transport.stop(discardRecordings, allowLooping);
transport.setPosition(timeInSeconds);
transport.scrub(scrubSpeedMultiplier);

// Recording
transport.record(retakeMode, autoReturn);
transport.isRecording();

// State queries
transport.isPlaying();
transport.getCurrentPosition();
transport.getPosition();
```

### Loop Control

```cpp
// Set loop range
transport.setLoopRange({ loopStart, loopEnd });
transport.looping = true;

// Query loop
auto loopRange = transport.getLoopRange();
bool isLooping = transport.looping;
```

### Transport Listeners

```cpp
struct MyListener : te::TransportControl::Listener
{
    void playbackContextChanged() override
    {
        // Called when playback starts/stops
    }

    void autoSaveNow() override
    {
        // Called when auto-save should occur
    }
};

transport.addListener(myListener);
```

## Tempo and Time

### TempoSequence

```cpp
auto& tempoSequence = edit->getTempoSequence();

// Set tempo
tempoSequence.setTempo(0.0, 120.0);  // 120 BPM at time 0
tempoSequence.setTempoAt(timeInSeconds, newBPM);

// Insert tempo points
tempoSequence.insertTempo(timeInSeconds, bpm);

// Time signatures
tempoSequence.insertTimeSig(timeInSeconds, numerator, denominator);
auto timeSig = tempoSequence.getTimeSig(timeInSeconds);

// Time conversions
auto beats = tempoSequence.timeToBeats(timeInSeconds);
auto time = tempoSequence.beatsToTime(beatNumber);

// Bar/beat conversions
auto barBeat = tempoSequence.toBarBeatFraction(beatNumber);
auto beats = tempoSequence.fromBarBeatFraction(barBeat);

// Timecode formatting
auto timecode = te::TimecodeDisplayFormat::formatTimecode(
    timeInSeconds, tempoSequence,
    te::TimecodeDisplayFormat::barsBeats);
```

### PitchSequence

```cpp
auto& pitchSeq = edit->getPitchSequence();

// Set key
pitchSeq.setPitchAt(timeInSeconds, pitch);  // 0=C, 1=C#, etc.

// Get current pitch
auto pitch = pitchSeq.getPitchAt(timeInSeconds);
```

## Device Management

### Audio Devices

```cpp
auto& deviceMgr = engine.getDeviceManager();

// Get devices
auto& audioOutput = deviceMgr.getDefaultWaveOutDevice();
auto& audioInput = deviceMgr.getDefaultWaveInDevice();

// Enable/disable
audioOutput.setEnabled(true);
audioInput.setEnabled(true);
```

### MIDI Devices

```cpp
// Get available MIDI devices
auto midiInDevices = te::MidiInputDevice::getAvailableDevices(engine);
auto midiOutDevices = te::MidiOutputDevice::getAvailableDevices(engine);

// Enable MIDI device
for (auto* midiIn : midiInDevices)
{
    midiIn->setEnabled(true);
    midiIn->setRecordingEnabled(*track, true);
}

// Send MIDI
midiOutDevice->sendMessage(midiMessage);
```

## Markers and Regions

### MarkerManager

```cpp
auto& markerMgr = edit->getMarkerManager();

// Add marker
auto marker = markerMgr.createMarker(-1, position, length, nullptr);
marker->name = "Verse 1";
marker->colour = juce::Colours::blue;

// Get markers
auto markers = markerMgr.getMarkers();

// Navigate
auto nextMarker = markerMgr.getNextMarker(currentTime);
auto prevMarker = markerMgr.getPrevMarker(currentTime);

// Delete marker
markerMgr.deleteMarker(marker, nullptr);
```

## Selection and Undo

### SelectionManager

```cpp
auto& selectionMgr = edit->getSelectionManager();

// Select objects
selectionMgr.selectOnly(clip);
selectionMgr.addToSelection(clip2);
selectionMgr.deselectAll();

// Query selection
auto selectedClips = selectionMgr.getItemsOfType<te::Clip>();
auto selectedTracks = selectionMgr.getItemsOfType<te::Track>();
```

### UndoManager

```cpp
auto& undoManager = edit->getUndoManager();

// Create transactions
undoManager.beginNewTransaction("Add Track");
// ... perform actions ...
undoManager.beginNewTransaction();  // Commits previous

// Undo/redo
undoManager.undo();
undoManager.redo();
undoManager.canUndo();
undoManager.canRedo();

// Clear history
undoManager.clearUndoHistory();
```
