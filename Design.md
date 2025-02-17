# MoTool Design Document

MoTool is a specialized demoscene production tool that bridges modern production workflows with retro platform
constraints. It combines DAW-like music editing, motion graphics prototyping, and direct machine code development
in a unified timeline-based interface.

## Background & Philosophy

MoTool emerges from a personal journey that spans both old-school demo coding and modern digital production.
Starting with ZX Spectrum assembly in the early 90s, through the evolution of web design and development, and back
to the demoscene in 2017. So this tool represents a bridge between worlds. Forged through experiences with
ZX Spectrum demo coding and with a number of Python prototypes, MoTool aims to solve a fundamental challenge:
how to bring modern streamlined production workflows to retro demo development without losing the direct connection
to the hardware.

The tool's architecture reflects lessons learned from various prototypes - from Python's rapid development capabilities
to JUCE's robust audio plugin framework and solid architecture. It particularly emphasizes the need for visibility into
intermediate stages of demo development, born from experiences developing complex chiptune VST plugins where debugging
multi-stage effects became crucial.

## Core Concepts

### Timeline-Centric Workflow

The timeline is the heart of MoTool, providing a unified view of all demo elements:

- Dual time rulers: musical (Bars/Beats/frames) and metric (mm:ss:frames)
- Vertical track stacking with clips representing different elements (music, fx, code)
- Tracks can be freely organized, grouped, and hidden
- Smart track creation when dragging clips to empty space

### Modern → Retro Pipeline

Each aspect of demo production supports a natural progression from modern prototyping to retro implementation:

#### Music Pipeline

1. Prototyping Layer
   - Standard MIDI/Audio tracks for mood reference and composition sketching
   - VST/AU plugin support for placeholder arrangements
2. Chiptune Layer
   - Instrument tracks with chip-aware parameters
   - Drum racks for retro percussion workflows
   - Direct import from Vortex Tracker/PT3 formats
3. Hardware Layer
   - Chip mixing track for register-level control
   - PSG track with direct register manipulation
   - Real-time chip emulation

#### Graphics/FX Pipeline

1. Prototyping Layer
   - Video/image import for quick visualization
   - Modern effects (transforms, color, etc.)
   - Shape/text tools for basic graphics
2. Development Layer
   - ScriptFX clips for effect prototyping
   - Live coding with instant feedback
   - AI-assisted graphics conversion to target formats
3. Hardware Layer
   - ASM/binary code clips
   - Direct memory/port access
   - State caching for fast reliable and controllable playback

### Node System Integration

- Split view with 1:1 timeline-node correspondence
- Node connections visible in both views
- Parameters can be automated in timeline or node view
- Live parameter linking through the pipeline

### Machine Integration

- Real-time emulation with state caching
- Snapshot system for reliable playback
  - Configurable snapshot frequency
  - Cache size management
- Memory/port access for scripts and ASM
- Multiple viewports for comparison

## Collaboration & Project Management

### Source Control Friendly

- Project structure optimized for git-based workflows
- Separate edit files for music and fx/nodes composition
- Asset watching for automatic updates
- XML-based project format for easier merging

### Project Organization

- Clear separation between source files and compiled assets
- Modular project structure for parallel development
- Shared timeline markers and tempo references
- Version control friendly project format

## Key Features

### Live Development

- Live coding in ScriptFX and AsmFX clips
  - Static mode (single frame updates)
  - Live mode (continuous updates)
  - Loop mode (timeline region repeating)
- Direct machine access via script API
  - Memory read/write
  - Port I/O
  - Graphics/sound chip control
- Real-time preview

### Automation & Sync

- Free parameter system for cross-track sync
- Timeline markers in both time domains
- Parameter linking between prototype and final FX clips and tracks
- Trajectory-based animation system

### Export Pipeline

- Compilation of ASM/scripts
- Resource conversion to target formats
- State/timeline export in Demo Prod Description Language
- Final binary bundle generation

## UI/UX Philosophy

- Multiple ways to achieve tasks (keys, menus, tools)
- Consistent behavior across tools
- Quick access to common operations
- Clear visual feedback
- Flexible snap behavior

## Future Considerations

- Enhanced node-timeline integration
- Additional retro platforms
- Extended live coding capabilities
- Community plugin system

# Video FX System Design for MoTool

## Background

The demoscene on retro platforms like ZX Spectrum requires complex video effects synchronized with music. Traditional development involves:

- Writing effects directly in assembly
- Manual synchronization with music
- Limited ability to prototype or iterate

Modern tools like video editors and motion graphics software offer powerful prototyping but lack:

- Integration with retro platform constraints
- Direct machine code development
- Real-time preview of actual hardware behavior

## Problem Statement

MoTool needs to bridge these worlds by providing:

1. Modern timeline-based editing for rapid prototyping
2. Direct integration with retro platform development
3. Real-time preview of effects
4. Parameter automation and synchronization with music

Key technical requirements:

- Multiple effect types (shaders, scripts, emulator output)
- 256x192 resolution, 16 colors
- Up to 50 FPS performance
- Frame-accurate synchronization with audio
- State management for emulator
- Timeline-based editing

## Existing Architecture Analysis

Tracktion Engine provides:

1. Robust timeline editing
2. Parameter automation
3. Plugin architecture
4. Real-time processing graph

These capabilities align perfectly with our needs, but require extension for:

- Video processing
- Emulator state management
- Multiple data types beyond audio/MIDI

## Design Solution

### Core Architecture

The design extends Tracktion's existing architecture while maintaining its core principles:

1. Timeline Layer:

   ```text
   Edit
   └─ FXTrack
         └─ FXClip
               └─ FXPlugin
   ```

2. Processing Layer:

   ```text
   EditPlaybackContext
   └─ NodeGraph
         ├─ EmulatorNode
         ├─ ShaderNode
         └─ ScriptNode
   ```

3. Data Layer:
   - Type-safe ports for different data types
   - Frame and state buffering
   - Parameter automation storage

This mirrors Tracktion's proven audio/MIDI architecture while adding video capabilities.

### Key Design Decisions

1. Plugin-Based Architecture
   - Why: Leverages Tracktion's existing parameter automation and state management
   - Alternative considered: Direct clip processing
     - Rejected due to loss of parameter automation and preset capabilities

2. Node Graph Processing
   - Why: Enables flexible routing of different data types
   - Alternative considered: Direct frame processing chain
     - Rejected due to complexity in handling multiple input/output types

3. Port System
   - Why: Type-safe connections between nodes
   - Alternative considered: Generic data passing
     - Rejected due to type safety concerns and debugging difficulty

### Implementation Strategy

1. Base Infrastructure:
   - FXNode base class
   - Port system
   - Basic parameter automation

2. Core Components:
   - EmulatorNode for machine emulation
   - ShaderNode for GPU effects
   - ScriptNode for custom processing

3. Integration:
   - Timeline editing
   - Real-time preview
   - Parameter automation

## Validation

The design satisfies our requirements:

1. Rapid Prototyping:
   - Timeline-based editing
   - Real-time preview
   - Parameter automation

2. Retro Platform Integration:
   - Direct emulator integration
   - Machine code development
   - State management

3. Performance:
   - Graph-based processing is efficient
   - Type-safe connections minimize overhead
   - Reuses proven Tracktion infrastructure

## Technical Details

### Port System

```cpp
enum class PortType {
    Audio,
    Video,
    Memory,
    DataStream
};

struct Port {
    juce::String name;
    PortType type;
    bool isInput;
    PortFormat format;
};
```

### Node Base Class

```cpp
class FXNode : public tracktion_graph::Node {
    virtual std::vector<Port> getPorts() const = 0;
    virtual void process() = 0;
};
```

## Next Steps

1. Implementation Priority:
   1. Port system and base node infrastructure
   2. Basic EmulatorNode implementation
   3. Parameter automation integration
   4. ShaderNode development

2. Future Considerations:
   - Frame caching strategy
   - Enhanced buffering system
   - Advanced node types

## Conclusion

This design:
- Leverages proven Tracktion architecture
- Adds video capabilities naturally
- Maintains flexibility for future extensions
- Meets all current requirements
- Provides clear implementation path

The solution is optimal given our constraints and requirements, building on existing architecture while adding necessary capabilities for demoscene production.
