# Clip Launcher System

Complete guide to Tracktion Engine's clip launcher system (v3.0+), providing Ableton Live-style clip launching and scene management.

## Overview

The clip launcher system allows non-linear, performance-oriented playback where clips can be triggered independently or in coordinated scenes. Each track contains a list of clip slots that can be filled with clips and launched on demand.

## Core Concepts

**ClipSlot**: A container for a clip that can be launched independently
**Scene**: A horizontal row of clip slots across multiple tracks that can be launched together
**FollowAction**: Automated sequencing that determines what happens after a clip finishes
**LaunchQuantisation**: Timing constraints for when clips start playing

## ClipSlot Basics

### Creating and Accessing Clip Slots

```cpp
// Get clip slot list for a track
auto track = edit.getAudioTracks()[0];
auto* clipSlots = track->getClipSlotList();

// Create new clip slot
auto* slot = clipSlots->insertNewClip(slotIndex);  // -1 = append

// Set clip in slot
slot->setClip(myClip);

// Get number of slots
int numSlots = clipSlots->size();

// Access specific slot
if (auto* slot = clipSlots->getClipSlot(slotIndex))
{
    // Use slot
}
```

### ClipSlot Properties

```cpp
// Clip assignment
slot->setClip(clip);
auto* clip = slot->getClip();

// Length override (if different from clip length)
slot->setLength(4.0);  // 4 beats
auto length = slot->getLength();

// Slot state queries
bool hasClip = slot->hasClip();
bool isPlaying = slot->isPlaying();
bool isQueued = slot->isQueued();
bool isRecording = slot->isRecording();
```

### Launching Clips

```cpp
// Launch clip immediately (respects quantisation)
slot->launch();

// Stop clip
slot->stop();

// Trigger (launch if stopped, stop if playing)
slot->trigger();

// Force immediate launch (ignore quantisation)
slot->launchImmediately();
```

## Launch Quantisation

Control when launched clips actually start playing:

```cpp
// Set quantisation for clip slot
auto& quant = slot->getLaunchQuantisation();

// Quantisation types
quant.setType(te::LaunchQuantisation::Type::none);        // Immediate
quant.setType(te::LaunchQuantisation::Type::bar);         // Next bar
quant.setType(te::LaunchQuantisation::Type::beat);        // Next beat
quant.setType(te::LaunchQuantisation::Type::halfNote);    // Next half note
quant.setType(te::LaunchQuantisation::Type::quarterNote); // Next quarter
quant.setType(te::LaunchQuantisation::Type::eighthNote);  // Next eighth
quant.setType(te::LaunchQuantisation::Type::sixteenthNote); // Next sixteenth

// Global launch quantisation (applies to all slots by default)
auto& globalQuant = edit.getLaunchQuantisation();
globalQuant.setType(te::LaunchQuantisation::Type::bar);
```

## Follow Actions

Follow actions determine what happens automatically when a clip finishes playing:

### Basic Follow Actions

```cpp
// Access follow actions for a clip slot
auto& followActions = slot->getFollowActions();

// Set what happens after clip plays
followActions.setFollowAction(te::FollowActions::Action::stop);
followActions.setFollowAction(te::FollowActions::Action::playNext);
followActions.setFollowAction(te::FollowActions::Action::playPrevious);
followActions.setFollowAction(te::FollowActions::Action::playFirst);
followActions.setFollowAction(te::FollowActions::Action::playLast);
followActions.setFollowAction(te::FollowActions::Action::playAny);  // Random
followActions.setFollowAction(te::FollowActions::Action::playOther); // Random excluding current
followActions.setFollowAction(te::FollowActions::Action::playAgain); // Loop current
```

### Follow Action Timing

```cpp
// When to trigger follow action
followActions.setFollowTime(4.0);  // After 4 beats

// Use clip length vs. custom time
followActions.setUseClipLength(true);   // Follow after clip ends
followActions.setUseClipLength(false);  // Follow after custom time

// Probability (for random actions)
followActions.setProbability(0.75);  // 75% chance
```

### Advanced Follow Actions

```cpp
// Multiple follow actions with different probabilities
followActions.setFollowActionA(te::FollowActions::Action::playNext);
followActions.setFollowActionB(te::FollowActions::Action::playAgain);
followActions.setProbabilityA(0.7);  // 70% play next
followActions.setProbabilityB(0.3);  // 30% play again

// Count-based triggering
followActions.setFollowCount(4);  // Trigger after 4 repetitions
followActions.setUseFollowCount(true);
```

## Scenes

Scenes allow launching multiple clips across tracks simultaneously:

### Creating and Managing Scenes

```cpp
// Get scene list from edit
auto* sceneList = edit.getClipSlotList().getSceneList();

// Create scene
auto* scene = sceneList->createScene(sceneIndex);  // -1 = append
scene->setName("Verse 1");

// Get scenes
int numScenes = sceneList->getNumScenes();
auto* scene = sceneList->getScene(sceneIndex);

// Delete scene
sceneList->deleteScene(sceneIndex);
```

### Launching Scenes

```cpp
// Launch all clips in scene
scene->launch();

// Stop all clips in scene
scene->stop();

// Launch scene with custom quantisation
scene->launchWithQuantisation(te::LaunchQuantisation::Type::bar);
```

### Scene Properties

```cpp
// Scene name and color
scene->setName("Chorus");
scene->setColour(juce::Colours::blue);

// Get clips in scene
for (auto* slot : scene->getClipSlots())
{
    if (slot->hasClip())
    {
        // Process clip
    }
}
```

## Recording to Clip Slots

### Audio Recording

```cpp
// Enable recording to clip slot
slot->setRecording(true);

// Start transport recording
edit.getTransport().record(false);

// After recording stops, clip is automatically in slot
auto* recordedClip = slot->getClip();
```

### MIDI Recording

```cpp
// Record MIDI to clip slot
auto midiTrack = dynamic_cast<te::MidiTrack*>(track);
if (midiTrack)
{
    // Enable input monitoring
    midiTrack->setInputMonitoringEnabled(true);

    // Arm slot for recording
    slot->setRecording(true);

    // Start recording
    edit.getTransport().record(false);
}
```

### Overdub/Replace Modes

```cpp
// Set recording mode
slot->setRecordingMode(te::ClipSlot::RecordingMode::replace);  // Replace existing
slot->setRecordingMode(te::ClipSlot::RecordingMode::overdub);  // Layer on top
```

## Complete Launcher Grid Example

```cpp
class ClipLauncher
{
public:
    ClipLauncher(te::Edit& edit, int numTracks, int numScenes)
        : edit(edit)
    {
        // Create tracks with clip slots
        for (int t = 0; t < numTracks; ++t)
        {
            auto track = te::createTrack<te::AudioTrack>(edit);
            track->setName("Track " + juce::String(t + 1));

            auto* clipSlots = track->getClipSlotList();

            // Pre-create slots
            for (int s = 0; s < numScenes; ++s)
                clipSlots->insertNewClip(-1);

            tracks.add(track);
        }

        // Create scenes
        auto* sceneList = edit.getClipSlotList().getSceneList();
        for (int s = 0; s < numScenes; ++s)
        {
            auto* scene = sceneList->createScene(-1);
            scene->setName("Scene " + juce::String(s + 1));
        }
    }

    // Load audio file into slot
    void loadAudioClip(int trackIndex, int slotIndex, const juce::File& audioFile)
    {
        if (auto track = tracks[trackIndex])
        {
            auto* clipSlots = track->getClipSlotList();
            if (auto* slot = clipSlots->getClipSlot(slotIndex))
            {
                // Create audio clip
                te::AudioFile file(edit.engine, audioFile);
                auto lengthInSeconds = file.getLengthInSeconds();

                if (auto clip = track->insertWaveClip(
                    audioFile.getFileNameWithoutExtension(),
                    audioFile,
                    { {0.0, lengthInSeconds}, 0.0 },
                    false))
                {
                    slot->setClip(clip);
                }
            }
        }
    }

    // Launch clip at position
    void launchClip(int trackIndex, int slotIndex)
    {
        if (auto track = tracks[trackIndex])
        {
            auto* clipSlots = track->getClipSlotList();
            if (auto* slot = clipSlots->getClipSlot(slotIndex))
                slot->launch();
        }
    }

    // Launch entire scene
    void launchScene(int sceneIndex)
    {
        auto* sceneList = edit.getClipSlotList().getSceneList();
        if (auto* scene = sceneList->getScene(sceneIndex))
            scene->launch();
    }

    // Stop track
    void stopTrack(int trackIndex)
    {
        if (auto track = tracks[trackIndex])
        {
            auto* clipSlots = track->getClipSlotList();
            for (int i = 0; i < clipSlots->size(); ++i)
            {
                if (auto* slot = clipSlots->getClipSlot(i))
                    slot->stop();
            }
        }
    }

    // Stop all
    void stopAll()
    {
        for (auto track : tracks)
            stopTrack(tracks.indexOf(track));
    }

private:
    te::Edit& edit;
    juce::ReferenceCountedArray<te::Track> tracks;
};
```

## Performance Patterns

### Sequential Pattern with Follow Actions

```cpp
// Create 4-bar loop pattern that progresses through variations
void setupProgressivePattern(te::Track& track)
{
    auto* slots = track.getClipSlotList();

    // Slot 0: Intro (plays once, then next)
    auto* intro = slots->getClipSlot(0);
    intro->getFollowActions().setFollowAction(te::FollowActions::Action::playNext);
    intro->getFollowActions().setFollowCount(1);
    intro->getFollowActions().setUseFollowCount(true);

    // Slot 1: Main pattern (loops 4 times, then next)
    auto* main = slots->getClipSlot(1);
    main->getFollowActions().setFollowAction(te::FollowActions::Action::playAgain);
    main->getFollowActions().setFollowCount(4);
    main->getFollowActions().setUseFollowCount(true);
    main->getFollowActions().setNextFollowAction(te::FollowActions::Action::playNext);

    // Slot 2: Variation (plays twice, back to slot 1)
    auto* variation = slots->getClipSlot(2);
    variation->getFollowActions().setFollowAction(te::FollowActions::Action::playPrevious);
    variation->getFollowActions().setFollowCount(2);
    variation->getFollowActions().setUseFollowCount(true);
}
```

### Generative Pattern with Randomization

```cpp
// Setup random pattern selection
void setupGenerativePattern(te::Track& track)
{
    auto* slots = track.getClipSlotList();

    for (int i = 0; i < slots->size(); ++i)
    {
        if (auto* slot = slots->getClipSlot(i))
        {
            auto& fa = slot->getFollowActions();

            // 70% chance: play another random clip
            // 30% chance: repeat current clip
            fa.setFollowActionA(te::FollowActions::Action::playOther);
            fa.setFollowActionB(te::FollowActions::Action::playAgain);
            fa.setProbabilityA(0.7);
            fa.setProbabilityB(0.3);

            // Trigger after clip length
            fa.setUseClipLength(true);
        }
    }
}
```

## Listener Pattern for UI Updates

```cpp
struct ClipSlotListener : te::ClipSlot::Listener
{
    void clipSlotChanged(te::ClipSlot& slot) override
    {
        // Clip assigned/removed
        updateUI();
    }

    void clipSlotPlayingStateChanged(te::ClipSlot& slot) override
    {
        // Playing state changed
        if (slot.isPlaying())
            highlightSlot();
        else
            unhighlightSlot();
    }
};

// Attach listener
clipSlot->addListener(myListener);
```

## Integration with Timeline/Arranger

### Switching Between Launcher and Arranger

```cpp
// Launcher mode (clips play from slots)
edit.setLauncherMode(true);

// Arranger mode (timeline playback)
edit.setLauncherMode(false);

// Query current mode
bool inLauncherMode = edit.isInLauncherMode();

// Hybrid mode: some tracks in launcher, some in arranger
track->setLauncherMode(true);  // Track uses clip slots
track->setLauncherMode(false); // Track uses timeline clips
```

### Converting Between Launcher and Arranger

```cpp
// Flatten launcher clips to timeline
void flattenToTimeline(te::Track& track)
{
    auto* clipSlots = track.getClipSlotList();
    double currentTime = 0.0;

    for (int i = 0; i < clipSlots->size(); ++i)
    {
        if (auto* slot = clipSlots->getClipSlot(i))
        {
            if (auto* clip = slot->getClip())
            {
                auto length = clip->getPosition().getLength();

                // Duplicate clip to timeline
                auto newClip = clip->clone(edit);
                newClip->setStart(currentTime, false, false);

                currentTime += length.inSeconds();
            }
        }
    }
}
```

## Best Practices

1. **Quantisation**: Always set appropriate launch quantisation for tight timing
2. **Follow Actions**: Use count-based follow actions for structured progressions
3. **Scene Organization**: Group related clips in scenes for coherent performances
4. **Slot Management**: Pre-create slots for consistent grid layout
5. **Color Coding**: Use colors to visually organize clips by type/function
6. **Recording**: Set recording mode appropriately (replace vs. overdub)
7. **Monitoring**: Enable input monitoring for MIDI/audio feedback during setup

## Common Patterns

### Live Performance Rig

```cpp
// Setup: 8 tracks × 8 scenes
// - Tracks 1-4: Drums, Bass, Keys, Lead (loops)
// - Tracks 5-6: One-shot samples
// - Tracks 7-8: Effects/transitions

ClipLauncher launcher(edit, 8, 8);

// Set global quantisation
edit.getLaunchQuantisation().setType(te::LaunchQuantisation::Type::bar);

// Load drum loops in track 0
for (int scene = 0; scene < 8; ++scene)
    launcher.loadAudioClip(0, scene, drumLoopFiles[scene]);

// Setup transitions on track 7 to auto-advance scenes
auto track7 = edit.getAudioTracks()[7];
for (int i = 0; i < 8; ++i)
{
    auto* slot = track7->getClipSlotList()->getClipSlot(i);
    slot->getFollowActions().setFollowAction(
        te::FollowActions::Action::playNext);
}
```

## API Reference Summary

**ClipSlot Methods:**
- `setClip()`, `getClip()`, `hasClip()`
- `launch()`, `stop()`, `trigger()`, `launchImmediately()`
- `isPlaying()`, `isQueued()`, `isRecording()`
- `setLength()`, `getLength()`
- `getLaunchQuantisation()`, `getFollowActions()`

**Scene Methods:**
- `launch()`, `stop()`
- `setName()`, `getName()`, `setColour()`
- `getClipSlots()`

**FollowActions Methods:**
- `setFollowAction()`, `setFollowActionA/B()`
- `setProbability()`, `setProbabilityA/B()`
- `setFollowTime()`, `setUseClipLength()`
- `setFollowCount()`, `setUseFollowCount()`

**LaunchQuantisation Types:**
- `none`, `bar`, `beat`, `halfNote`, `quarterNote`, `eighthNote`, `sixteenthNote`
