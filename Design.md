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

## Video FX System Design for MoTool

### Problem Statement

MoTool needs to support multiple types of video effects for demoscene production:

- Real-time shader effects (because why not, let's introduce the concept with well-known example)
- Scriptable effects (Python/Cython/ChaiScript)
- Emulator output with both video and audio
- Multiple effects composited together
- Timeline-based editing with automation

Key challenges:

1. Synchronization between audio and video
2. Real-time preview during editing
3. Parameter automation per clip
4. Efficient frame buffering and caching
5. State management for emulator rewind/seeking
6. Multiple data types (video frames, memory buffers, data streams)

### Design Constraints

1. Technical Constraints:
   - Fixed video resolution (256x192, 16 or 256 colors)
   - Target frame rate up to 50 FPS
   - Must support both video and audio for emulator
   - Must integrate with Tracktion Engine's timing system
2. Architectural Constraints:
   - Use existing Tracktion Engine infrastructure where possible
   - Support plugin-style effects with parameters
   - Allow mixing different effect types on timeline
   - Enable real-time parameter editing

### Proposed Solution

#### 1. Architecture Overview

The system is designed in three layers:

##### A. Edit Structure Layer

- FXTrack: Timeline track containing video effects
- FXClip: Individual effect instances on timeline
- FXPlugin: Effect implementation with parameters
- All integrated into Tracktion's existing Edit model

##### B. Processing Layer

- NodeGraph: Manages processing nodes and connections
- FXNode: Base class for effect processing
- Specialized nodes (EmulatorNode, VideoNode, ImageNode, TextNode, ShaderNode, ScriptNode)
- Port system for flexible data routing

##### C. Data Layer

- Multiple data types through typed ports
- Buffering system for video frames
- Memory management for emulator state
- Parameter automation storage

#### 2. Key Components

##### FXNode Base System

```cpp
class FXNode : public tracktion::graph::Node {
    virtual std::vector<Port> getPorts() const = 0;
    virtual void process() = 0;
};
```

##### Port System

Subject to change, now it is very roughly defined, only as a placeholder for further discussion.

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

##### Integration with Tracktion Engine

- Uses existing EditPlaybackContext for timing
- Leverages Plugin architecture for parameters
- Reuses automation system for parameters
- Parallels audio/MIDI processing model

#### 3. Processing Flow

1. During Edit Construction:
   - FXTracks are created in Edit
   - FXClips are added to tracks
   - Each clip creates its FXPlugin instance

2. During Playback:
   - EditPlaybackContext creates node graph
   - Plugins create their processing nodes
   - Nodes are connected based on ports
   - Graph processes frame by frame

3. Per-Frame Processing:
   - Get current time from transport
   - Update node parameters from automation
   - Process nodes in graph order
   - Output final frame to display

#### 4. Key Benefits

1. Leverages Existing Infrastructure:
   - Uses Tracktion's proven plugin system
   - Integrates with existing timing/automation
   - Follows familiar track/clip model

2. Flexible Effect System:
   - Support for multiple effect types
   - Extensible node/port system
   - Clean separation of concerns

3. Efficient Processing:
   - Graph-based processing
   - Type-safe port connections
   - Reusable node infrastructure

### Next Steps

1. Implementation Priorities:
   - Base FXNode and port system
   - Basic ShaderFX implementation
   - Integration with EditPlaybackContext
   - Parameter automation support

2. Future Considerations:
   - Frame caching strategy
   - Latency compensation
   - Multiple input/output support
   - Enhanced buffering system (?)

3. Open Questions:
   - Specific caching requirements
   - Real-time performance optimization
   - Advanced port metadata needs
   - Script engine integration details
