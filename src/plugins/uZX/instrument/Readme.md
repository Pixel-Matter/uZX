# ChipInstrumentPlugin Architecture

This document describes the architecture and class relationships of the ChipInstrumentPlugin system.

## Class Diagram

```mermaid
classDiagram
    class ChipInstrumentPlugin {
        <<Plugin>>
        -ChipInstrumentFx midiEffect
        -LevelMeasurer levelMeasurer
        -std::unordered_map paramLabels
        -bool flushingState
        +initialise()
        +deinitialise()
        +reset()
        +midiPanic()
        +getLevel(channel)
        +addParam()
        +valueTreePropertyChanged()
    }

    class MidiFxPluginBase~MIDIFX~ {
        <<Template>>
        #MIDIFX midiEffect
        +applyToBuffer(context)
        +takesMidiInput()
        +producesAudioWhenNoAudioInput()
    }

    class ChipInstrumentFx {
        <<MPE Instrument>>
    }

    class MPEInstrumentFx~Voice~ {
        <<Template>>
        -VoiceManager voices
        -MPEInstrument mpeInstrument
        +operator()(context)
        +renderNextSubBlock()
        +handleMidiEvent()
        +noteAdded()
        +noteReleased()
        +notePitchbendChanged()
        +notePressureChanged()
    }

    class VoiceManager~Voice~ {
        <<Template>>
        -OwnedArray voices
        -std::atomic shouldStealVoices
        -uint32 lastNoteOnCounter
        +addNote()
        +releaseNote()
        +findFreeVoice()
        +findVoiceToSteal()
        +turnOffAllVoices()
        +forEachActiveVoice()
    }

    class ChipInstrumentVoice {
        -te::LinEnvelope ampAdsr
        -te::LinEnvelope pitchAdsr
        -double pitchDepth
        -double pitchBendRange
        -bool triggerNote
        +noteStarted()
        +noteStopped()
        +renderNextStep()
        +renderNextBlock()
        +computeCurrentLevel()
        +computeCurrentNotePitch()
    }

    class MPEEffectVoice~Derived~ {
        <<Template>>
        #MPENote currentlyPlayingNote
        #double currentPlayRate
        #uint32 noteOnOrder
        +isActive()
        +isCurrentlyPlayingNote()
        +getCurrentlyPlayingNote()
        +clearCurrentNote()
    }

    class MidiBufferContext {
        +MidiMessageArray& buffer
        +int start
        +int length
        +TimePosition playPosition
        +double sampleRate
        +sliced()
        +getSampleForTimeRel()
        +processStartTime()
    }

    class Plugin {
        <<Tracktion>>
        +addParam()
        +getAutomatableParameters()
        +valueTreeChanged()
    }

    class MPEInstrument {
        <<JUCE>>
        +addListener()
        +processNextMidiEvent()
        +releaseAllNotes()
        +enableLegacyMode()
    }

    class MPEInstrumentListener {
        <<Interface>>
        +noteAdded()
        +noteReleased()
        +notePitchbendChanged()
        +notePressureChanged()
    }

    class LevelMeasurer {
        <<Tracktion>>
        +addClient()
        +getAndClearAudioLevel()
    }

    %% Inheritance relationships
    ChipInstrumentPlugin --|> MidiFxPluginBase
    MidiFxPluginBase --|> Plugin
    ChipInstrumentFx --|> MPEInstrumentFx
    MPEInstrumentFx --|> MPEInstrumentListener
    ChipInstrumentVoice --|> MPEEffectVoice

    %% Template specialization
    MidiFxPluginBase --> ChipInstrumentFx : template parameter
    MPEInstrumentFx --> ChipInstrumentVoice : template parameter
    VoiceManager --> ChipInstrumentVoice : template parameter
    MPEEffectVoice --> ChipInstrumentVoice : CRTP

    %% Composition relationships
    ChipInstrumentPlugin *-- ChipInstrumentFx : contains
    ChipInstrumentPlugin *-- LevelMeasurer : contains
    MPEInstrumentFx *-- VoiceManager : contains
    MPEInstrumentFx *-- MPEInstrument : contains
    VoiceManager *-- ChipInstrumentVoice : manages multiple

    %% Usage relationships
    MidiFxPluginBase ..> MidiBufferContext : uses
    MPEInstrumentFx ..> MidiBufferContext : processes
    ChipInstrumentVoice ..> MidiBufferContext : renders to

    %% Notes
    note for ChipInstrumentPlugin "Main plugin class\nExtends Tracktion Plugin\nHandles parameter automation\nManages level metering"

    note for ChipInstrumentFx "Simple wrapper around\nMPEInstrumentFx template"

    note for MPEInstrumentFx "Core MPE processing\nHandles MIDI event routing\nManages voice allocation\nProcesses MPE gestures"

    note for VoiceManager "Voice allocation & stealing\nPolyphony management\nThread-safe voice access"

    note for ChipInstrumentVoice "Individual note rendering\nADSR envelope processing\nMIDI output generation\nChiptune-specific synthesis"
```

## Architecture Overview

The ChipInstrumentPlugin follows a layered architecture:

1. **Plugin Layer**: `ChipInstrumentPlugin` extends `MidiFxPluginBase` and provides the Tracktion Engine plugin interface
2. **MIDI Effect Layer**: `ChipInstrumentFx` wraps the MPE instrument functionality
3. **MPE Processing Layer**: `MPEInstrumentFx` handles MPE events and voice management
4. **Voice Management Layer**: `VoiceManager` manages polyphonic voice allocation
5. **Voice Rendering Layer**: `ChipInstrumentVoice` renders individual notes with chiptune-specific synthesis

## Key Design Patterns

- **Template-based Composition**: Uses C++ templates for type-safe, zero-overhead composition
- **CRTP (Curiously Recurring Template Pattern)**: `MPEEffectVoice` uses CRTP for compile-time polymorphism
- **Observer Pattern**: MPE events are propagated through listener interfaces
- **Strategy Pattern**: Voice allocation and stealing algorithms are encapsulated in `VoiceManager`

## Data Flow

1. MIDI events enter through `ChipInstrumentPlugin::applyToBuffer()`
2. `MidiFxPluginBase` creates `MidiBufferContext` and calls the effect
3. `MPEInstrumentFx::operator()` processes MIDI events and manages voice allocation
4. Individual `ChipInstrumentVoice` instances render MIDI output with synthesis parameters
5. Final MIDI output is written back to the buffer for downstream processing