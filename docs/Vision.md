# MoTool is a planned modern PC-based *de*motool for retrocomputer demoscene production

```text
        █▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌
        █▌  █▌    █▌      █▌    █▌█▌    █▌
  █▌█▌█▌█▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌█▌█▌█▌
        █▌
                        |   |
    ,---.---.  ,---.  --|---|--  ,---.   ,---
    |   |   |       |   |   |   |    |  |
        |   |   ,---|   |   |   |---'   |
            |  |    |   |   |   |       |
               `---'    |   |    `---'
                        `-- `--

    d88o888ob.      d888888888b             d8b
    8@@8@@@8@@b ,d888b.`8@8',d888b. ,d888b. B@8
    B@8'B@8'8@8 8@8 8@8 B@8 8@8 8@8 8@8 8@8 B@8   Copyright 2025–2026
    B@8 B@8 B@8 '"" 8@8 B@8 8@b ""' '"" 8@8 B@8     pixelmatter.org
    Y@8 8@8 Y@@88@@@8P' 8@8 `Y@@@88888@@@P' Y8P
```

## ZX Spectrum-friendly logo

```text
    d8o88o.     d888888b          db
    8888888b ,o8o.`88',o8o. ,o8o. B8
    B8'B8'88 88'88 B8 88'88 88'88 B8
    B8 B8 B8 B8 88 B8 B8 B8 B8 B8 B8
    B8 B8 B8 B8 88 B8 B8 B8 B8 B8 B8
    B8 B8 B8 "" 8P B8 Yb "" "" 8P B8
    Y8 88 Y@888@8' 88 `8@88888@8' YP
```

Designed for easy prototyping.

Software for making demoscene productions for retrocomputers using Tracktion Engine and JUCE. User must be able to quickly build a prototype of a demo with placeholder midi and wave music, images with animated tranformations and builtin effects, edit it and gradually replace audio with music written for emulated target machine music chips, convert images to target graphics formats and write effects as machine code in Assembler. Final edit is transformed into binary packed bundles of code, music, graphics and data and run in retro machine emulator right in the demotool editor track like normal video. Featurewise it is like very specific video editor.

### Timeline mode

- Timeline consist of tracks one under the other, tracks are composed of clips.
- Tracks can be sorted and moved around, added and deleted
- Dragging a clip from track to empty space creates a new track of suitable type
- There are two time rulers: Bars/Beats/frames on top of the timeline and mm:ss:fr on bottom.
- Timeline clip tools and mouse zones:
  - Select clips
  - Cut clips
  - Time-stretch clips
  - Extend clips or move cut point between clips
- Some tracks can be hidden, for example, you can have visible only MIDI Instrument track, but between MIDI instrument and hearable audio there is:
  - Sliced MIDI stream track
  - Chip mixed MIDI stream track
  - Rendered audio track

### Track and clip types

#### Music composing mode tracks

µZX subsystem — like your favorite DAW.

#### FX composing mode track

This subsystem is like your favorite video/motion design/compositing editor. When you inster video with audio track in
it, audio is placed on separate audio track described above, timeline-linked to video.
When you compose your demo from real and prototyped effects, you always see audio track with the same labels and
regions. And vice versa, when composing music, you see fx track with clips and can adjust cuts, and automate and link
params.

- Automation track: track for syncronization of music and effects. You can add free params and link this params to any other automation track:
  be it music or fx track.
- Video/FX: for placeholder video clips, images, shapes, text and for ChaiScript, Python/Cython plugins, for video effects prototyping.
  With animated trasforms and video effects.
  - Clips:
    - VideoClip, any video or animation file: mp4, avi, apng, animated gifs, etc. With transparency support. Audio placed separately.
    - ImageClip, any image from a file or copypasted into: png, gif, jpg, src (ZX Spectrum screen format), etc. With transparency support.
    - ShapeClip, rectangles, circles
    - TextClip, text, with font chooser, color, size, adjusting, etc
    - RenderClip, 3D/2D object renderer clip. Details to be determined yet. For prototyping or data stream linking.
    - GizmoClip, just a handle to animate or place. Used to control FX clips.
    - ScriptFxClip, here comes the most interesting: ChaiScript, Python/Cython code snippet that handles effect.
      Can define params to automate or input/output vars and data buffers from other clips/nodes.
  - Common parameters: transforms, alpha, scale, pos, rotation, flips, etc
  - Plugins:
    - Common color plugins: levels, brightness, contrast, keying, alpha, etc
    - Transform plugins: crop, rotate, flip, etc
    - Converter plugins: for converting rendered frames to machine-specific graphics format.
  - Automation:
    - Transforms with trajectories
- Machine track: Machine type and machine settings. Machine emulated in realtime or while prerendering.
  You can step back, rewind of fast-forward an emulation! To be able to do so we cache snapshots of the machine state in key frames.
  (for example every 16th frame).
  - Clips:
    - FxClip: asm or binary code and data for emulated machine.
    - Parameters:
      - Preloaded snapshot or disk/tape image
      - Patches: list of data/code blocks to load into machine memory, can input vars and data buffers from other clips/nodes.
      - Code entypoints to execute on start (or on every frame).
      - List of variables (memory addresses) to automate or filename with generated labels with variables.
    - AsmFxClip: FxClip with source file, editor. Compiled into machine code.
    - CodeFxClip: FxClip with binary-ony code.
  - Output: Automated parameters data stream/fx changes/loading commands for the demoscene engine for target machine
    in some human/machine readable Demo Prod Description Language.
  - Export: compiled, buit from sources and linked result.

### Viewport

You have a display like in any video editor where you see the current frame, can play, stop, rewind, fast-forward and step frames back and forth.
Also you can move objects or gizmos around.

#### Node system

As you read from clips and tracks vision, some tracks and clips can be interlinked with params (vars) and data buffers, images can be converted,
then animated for prototyping, then animated params and image data should be used in AsmFxClip to build real machine code effect.

It is open question how to join concepts of a timeline edit mode and node system edit mode. It is unsolved industry problem yet, Even professional
video editing and motion design software struggles to implement this conceptual unification. My vision is to provide two panels: timeline and nodes,
with 1-1 correspondence between every clip in Timeline Panel and node in Nodes View. So when you select an image clip, corresponding image node is selected.
And vice versa. Links between nodes can be done as in Nodes View as between tracks in Timeline Panel (Cavalry-slyle?). Parameters animated in timeline view
or in properties panel (keyframe by keyframe).

Yes, there is some gaps in this vision, but we should eat the elephant by pieces.
