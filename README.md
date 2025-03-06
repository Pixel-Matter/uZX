# MoTool is a modern PC-based *de*motool for retrocomputer demoscene production

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
    8@@8@@@8@@b ,d888b.`8@8',d888b. ,d888b. B@8       Copyright 2025
    B@8'B@8'8@8 8@8 8@8 B@8 8@8 8@8 8@8 8@8 B@8    Ruslan Grokhovetski
    B@8 B@8 B@8 '"" 8@8 B@8 8@b ""' '"" 8@8 B@8    www.pixelmatter.org
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

![Tracktion Engine](third_party/tracktion_engine/tutorials/images/tracktion_engine_powered.png)

Powered by [Tracktion Engine](https://github.com/Tracktion/tracktion_engine/).

Designed for easy prototyping.

Software for making demoscene productions for retrocomputers using Tracktion Engine and JUCE. User must be able to quickly build a prototype of a demo with placeholder midi and wave music, images with animated tranformations and builtin effects, edit it and gradually replace audio with music written for emulated target machine music chips, convert images to target graphics formats and write effects as machine code in Assembler. Final edit is transformed into binary packed bundles of code, music, graphics and data and run in retro machine emulator right in the demotool editor track like normal video. Featurewise it is like very specific video editor.

### About me

I'm experienced C++ and Python developer with background in video editing, 3D, graphics and motion design in early 2000-s,
webdesign and UX in mid 2000-s, web backend in 2010-s.

This vision is being appeared in my mind for several years, with number of implemented prototypes of the tool,
in python mainly, in C++ JUCE VST plugin form, implemented machine code in a timeline with state caching already in Python,
bridging the FUSE emulator with C++ and pybind11, etc. Also I'm very into demoscene and coded several asm effects for ZX Spectrum,
winning places at compos. So I do not afraid, I just have to get myself, my small projects codebase and this vision together.

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

This subsystem is like your favorite DAW.

- Markers track: for labeling sections of the timeline. Actually this track is on the top/bottom rulers.
  Top labels are linked to bars/beats and do not shift when tempo/time signature changed, bottom ones are linked to seconds.
- Tempo/Time signature track — can be displayed on special track inside the ruler aslo. Tempo is constrained by target
- Audio track: for prototyping with audio placeholders or for converting to target machine formats.
  - Clips:
    - AudiocClip. Can be time-stretched, looped. When extended, loops are repeated.
    - ContainerAudioClip. You can group AudiocClips together.
  - Display: Waveforms.
  - Plugins: EQ, compressor/limiter, normalizer, delay, reverb, downsamling with dithering.
  - Automation:
    - Volume, pan.
    - Plugins parameters.
  - Input for record/monitor/chaining: Audio
  - Output: Audio
- MIDI track: for prototyping with midi placeholders or for downmixing MIDI to target machine PSG data
  - Clips:
    - MidiClip. Multimple MidiClips can share data, so you can edit a bassline once and every instance of it reflects the changes. You can add/remove/edit individual notes.
    - ContainerMidiClip. You can group several midi clips together.
  - Display: MIDI notes on pianoroll.
  - Plugins:
    - MIDI plugins: for processing MIDI events: arps, midi-emulated ADSR, LFOs, echo,  etc.
    - Synth plugins: regular ones and Chip emulation.
    - After synth plugins you can apply audio plugins as usual.
  - Automation:
    - Volume, pan.
    - Midi: you can add any MIDI CC, PitchWheel, Aftertouch lane and automate it.
    - Plugin paramters.
  - Input for record/monitor/chaining: MIDI
  - Output: MIDI, Audio (if audio plugins applied)
- Chiptune grouping track: track for grouping all of that is below:
- Instrument track: MIDI track with an "instrument": every input note transformed with instrument settings into sliced and automated MIDI stream,
  so to say MIDI-emulate volume ADSRs, LFOs, legato, portamento, detune, etc.
  Can operate with parameters only meaningful to specific music chips, for example with AY envelope modulation or noise.
  Aware of target music chip and its parameters.
  - Input for record/monitor/chaining: MIDI. For example, hardware/virtual MIDI keyboard.
  - Output: MIDI. Intended output is Chip Mixing track.
- Drum rack track: Special instrument track that maps every midi note to its separate instrument.
  - Input for record/monitor/chaining: MIDI. For example, hardware MIDI pads controller.
  - Output: MIDI. Intended output is Chip Mixing track.
- Chip Mixing track: downmixes instrument tracks to chip register vaues (PSG).
  Aware of target music chip and its parameters.
  - Plugins: ChipMixer for specific music chip(s).
  - Input for record/chaining: MIDI, intended chain input is all instrument tracks.
  - Output: MIDI, CC stream for specific music chip. Intended output is PSG track.
- PSG track: music data for playback on a target machine. PSG track is a MIDI track with CC values mapped to music chip registers.
  - Clips: MIDI clips with CC values mapped to music chip registers.
  - Display: notes on pianoroll by chip channels, notes can slide continously, color coded channels, with displayed notes volume and all modulation. And you can edit this.
  - Plugins: specific music chip emulator. You can change tuning, clock rate, fps, etc.
  - Automation:
    - Master volume.
    - Register values.
  - Input for record/chaining: MIDI. Intended chain input is Chip Mixing track.
  - Output:
    - MIDI, CC stream for specific music chip
    - Audio, (emulated chip output)
    - Data, PSG binary data for native/emulated player

#### FX composing mode track

This subsystem is like your favorite video/motion design/compositing editor. When you inster video with audio track in it,
audio is placed on separate audio track described above, timeline-linked to video.
When you compose your demo from real and prototyped effects, you always see audio track with the same labels and regions.
And vice versa, when composing music, you see fx track with clips and can adjust cuts, and automate and link params.

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

#### Some background

*When and how did I first get into the demoscene? What drew me to it and specifically to ZX Spectrum demos?*

I had ZX Spectrum Soviet clone in 1990 and falled in love with it, of course. Games, BASIC, first simple games programmed.
Then asm, self-taught, but not for very complex projects. There was not demoscene then yet. Then in early 90s in school and
University I switched to PCs, of course. VGA graphics is no comparison to ZX. I coded some semi-static 256 color fx
with Pascal, and some wireframe 3D.

*What are some of your favorite demos that inspired you, either technically or artistically? Are there specific effects
or techniques that you found particularly fascinating?*

Then I saw Second Reality and it blew my mind. Music and fx! Impossible colorful plasma! You know! But I didn't know
how to code this tricks in Pascal and x86 asm, and didn't have much time. I have to work and study. Then I got to FIDO
and then internet, and decided to involve into that, design, web programming etc. But later, in 2017 as I can remember
I realised that web turned into something very complex and not fun. So I remembered good old days when you knew the
machine from top to bottom and could push it to the limits, deepening your understanding etc. I found out that
ZX Spectrum demoscene lives and started to draw pictures in 6912 byte format, you know.

*Mentioned winning compo places with ZX Spectrum effects - could you tell me about some of these demos/effects?
What made them unique or challenging?*

I posted pictures on zxart.ee, joined ZX telegram discussion groups on gfx and music (Also I play guitar and synth,
a little). Then made friends with some forks from ZX scene, virtually, then in DI:Halt demoparty irl, when I took 1st
place for "Little PRINC-E" ZX Spectrum graphics. Then we coded 256byte intro Stellarator, first place at 2019 CAFEParty.
Then one more 256 byte intro, no top place. I wanted to make my first full-featured demo prod, but alas my first
concepts of a demo didnt driven anyone in our little group. Then in 2022 the war happened and motivation for the demo
dropped, but I have some demoscene friend from London, we developed very ambitious fx projects and conversion tools
involving AI and NN (I have some backgound on it too). So to this time I gathered a number of semi-finished conversion
and tooling projects, semi-finished intrersting asm fx, an ambitious demo vision even, but no team alas. And to start
with music, I should have a good instrument, and I do not like oldschool trackers. So, this vision came naturally.
But maybe I'm just a person that can not finish anything?

*On the development side:*

*What led you to explore different prototypes in Python and C++?*

I do not know, It came naturally, as I said, I just love Python.
C++ is for performance, zero-abstraction, you know, elegance, etc.. and for tracktion engine. I had experience with
Python UI libraries and didn't like it very much. I do not want to get into Qt, its a monster and it is outdated now.

*Which aspects of JUCE particularly appealed to you for the VST plugin approach?*

Oh! I like ValueTreeState approach very much! And JUCE for multiplatform plugin development is very strong, no alternatives.

*What were the key lessons learned from these different implementation attempts?*

Lessons are: Python is very flexible and is very good for rapid development, but absence of strong typing can mislead
into bad design, and Python lacks good enough UI frameworks. I used GTK, PyImgGUI, what else i can not remember,
but JUCE with C++ is a win. And regarding software packaging, Python have problems, you know.

When I developed AY chip VST3 plugin, I started very quickly and get results, but when I added features, legato, portamento,
mixing, pitchwheel, all in one plugin, I struggled with complexity and wanted to break the plugin into separate parts
to debug and inspect the intermidiate midi streams separately. So again it came naturally to implement my own DAW-like tool for that.

*Your background spans development, video editing, design, and UX - how do you think this interdisciplinary experience*
*shapes your vision for MoTool? Are there specific pain points from these different fields that you're hoping to solve?*

I do not know really, I didn't think of that specifically. But I can not start a demo without a music, and can not start
composing music in trackers. Integration and iteration, maybe this is the word.
