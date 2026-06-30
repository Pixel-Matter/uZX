# Plan: Video clips on video tracks, synced & audio-delay-aware

## Context

uZX is a PSG music editor built on JUCE + tracktion_engine. The goal is to add **video
clips** that live on a timeline lane, arranged over time (multiple clips, sequential/
overlapping), with a **video viewer panel** that renders the current frame, kept in sync
with the audio playhead and **compensated for audio output latency**. Targets: macOS,
Windows, and Linux (Linux has no JUCE video backend, so the feature must compile out /
degrade gracefully there).

### Why this approach

Investigation surfaced three decisive facts:

1. **tracktion's own video clip type is an unimplemented placeholder** (`TrackItem::Type::video`
   is marked "not yet implemented") and `TRACKTION_ENABLE_VIDEO=0` is hard-set in
   `src/CMakeLists.txt:223`. We will **not** rely on tracktion's video clip path.

2. **The engine already exposes everything needed for sync without a video clip type:**
   `TransportControl::Listener` has built-in `setVideoPosition(TimePosition, forceJump)`,
   `startVideo()`, `stopVideo()` callbacks (`tracktion_TransportControl.h:335-342`), and
   `EditPlaybackContext::getAudibleTimelineTime()` / `getLatencySamples()` give the
   latency-compensated timeline position. There is even a TODO in our own
   `EditState.cpp:154 handlePlaybackScrolling()` noting we *should* switch to
   `getAudibleTimelineTime()` for latency.

3. **The codebase already has a custom-clip extension mechanism** (authored by the repo
   owner). The tracktion fork is patched at `tracktion_Clip.cpp:228-232` to fall back to
   `EngineBehaviour::createCustomClipForState()` for unknown clip XML types. `PsgClip`
   uses this exact path. A **custom track subclass is a dead end** — `PsgTrack.test.cpp:89`
   documents that `Edit::createTrack` cannot construct custom track types. So tracks stay
   plain `te::AudioTrack`; clips are the custom type.

**Therefore:** add `VideoClip` as a first-class custom tracktion clip (mirroring `PsgClip`'s
extension wiring), host clips on ordinary audio tracks, render the timeline block with a
`VideoClipComponent`, and drive a `juce::VideoComponent`-based viewer from the transport's
video-sync callbacks + the audible (latency-compensated) timeline position. The authoritative
clock is the engine.

> **Two refinements baked into this plan:**
>
> 1. **`VideoClip` derives from `te::Clip`, NOT `te::AudioClipBase`/`WaveAudioClip`.**
>    `AudioClipBase` means "this clip contributes audio nodes to tracktion's graph" — wrong
>    for video. **Proof in the graph builder:** `tracktion_EditNodeBuilder.cpp:879` does
>    `if (auto audioClip = dynamic_cast<AudioClipBase*>(&clip)) return createNodeForAudioClip(...)`,
>    so any `AudioClipBase` is force-routed into audio-node creation (and pulls in audio proxy
>    rendering, ARA, waveform analysis, audio fades, audio export). A video clip must avoid all
>    of that. Precedent for a non-audio clip: `MarkerClip`, `ChordClip`, `ArrangerClip` derive
>    directly from `Clip`, whose pure-virtual set is small (`isMidi`, `canBeAddedTo`,
>    `isMuted`, `getDefaultColour`). (The earlier `AudioClipBase` choice was wrong — it injects
>    silent audio nodes and forces stubbed audio-rendering virtuals.)
>
> 2. **Linked-sibling audio model.** A video import creates a `VideoClip` (picture) PLUS one
>    or more `WaveAudioClip`s (embedded audio), tied by a shared `syncGroupId`. The audio
>    plays through tracktion's normal graph (latency comp, plugins, mixing, **export**); the
>    video display's own audio is muted (`setAudioVolume(0.0f)`). NLE-correct, and strictly
>    better than discarding the video's audio. (Demux/extract is a real dependency — see the
>    decode-backend open decision in the FX addendum; a picture-only v1 with audio-linking as
>    a fast-follow is acceptable.)

---

## Design overview

```
Video file asset (one import)
  ├─ VideoClip (custom clip : te::Clip, IDs::VIDEOCLIP)  ── on a "video" AudioTrack
  │     • assetId/file, sourceStart, sourceLength,           (plain te::AudioTrack — no
  │       timelineStart, speed, syncGroupId                   custom track class; see note)
  │     • adds NO audio nodes to the graph
  │     • drawn by VideoClipComponent          ───────────►  VideoViewerPanel (juce::Component)
  │                                                            ├─ juce::VideoComponent (audio muted)
  └─ WaveAudioClip(s) (embedded audio) ── on an audio track   └─ TransportControl::Listener
        • normal tracktion audio: latency comp,                    setVideoPosition/start/stopVideo
          plugins, mixing, export                              + Timer reading getAudibleTimelineTime()
        • same syncGroupId as the VideoClip
```

- **Model:** `VideoClip` (derives from `te::Clip`) holds the picture stream + source/timeline
  timing metadata and a `syncGroupId`; it contributes **no audio nodes**. Embedded audio is
  separate sibling `WaveAudioClip`(s) sharing the same `syncGroupId`. Multiple `VideoClip`s
  on the timeline are supported natively (each is a clip).
- **Timeline lane:** a "video track" is a **plain `te::AudioTrack`** holding video clips (no
  custom track subclass — `Edit::createTrack` can't build those; `PsgTrack.test.cpp:89`).
  Video clips on it render via a new `VideoClipComponent`.
- **Viewer:** a single `VideoViewerPanel` owns one `juce::VideoComponent`. As the playhead
  crosses clip boundaries it loads the active clip's file and seeks; outside any clip it
  shows blank. Sync is push (transport callbacks) + pull (timer @ ~30 Hz reading audible
  time and calling `setPlayPosition`), with drift correction.
- **Latency awareness:** the viewer seeks to `EditPlaybackContext::getAudibleTimelineTime()`
  (falls back to `getTransport().getPosition()` when no context), so the displayed frame
  matches what is *audibly* playing, not the raw transport position.
- **Platform gating:** all video code compiled under `#if MOTOOL_VIDEO_ENABLED` (true on
  macOS/Windows, false on Linux). On Linux the viewer shows a "video not supported on this
  platform" placeholder and clips still exist as model/timeline blocks (no playback).

---

## Implementation steps

### 1. Build: enable juce_video (macOS/Windows only)

- `src/CMakeLists.txt`:
  - Add `juce::juce_video` to `COMMON_APP_LINK_LIBS` (currently ~line 238-248) — but only
    on non-Linux. Wrap with `if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")` and define a
    cache/compile flag `MOTOOL_VIDEO_ENABLED`.
  - Add `JUCE_MODULE_AVAILABLE_juce_video=1` to the `target_compile_definitions(motool_common PUBLIC ...)`
    block (~line 250-274), guarded the same way.
  - Add `target_compile_definitions(motool_common PUBLIC MOTOOL_VIDEO_ENABLED=$<BOOL:...>)`
    so all source can branch on it.
  - Leave `TRACKTION_ENABLE_VIDEO=0` as-is (we do not use tracktion's video path).
- The `juce_video` module already exists in the tree
  (`third_party/tracktion_engine/modules/juce/modules/juce_video/`); no submodule change.
- After editing CMake, regenerate (`cmake -S . -B build`) so clangd + JuceHeader pick up the
  new module (per CLAUDE.md clangd notes).

### 2. Model: `VideoClip` (mirror `PsgClip`)

New files `src/models/VideoClip.h` / `.cpp` and additions to existing files:

- `src/models/Ids.h`: add `DECLARE_ID(VIDEOCLIP)`, plus video clip-state ids:
  `videoSource` (file/assetId), `sourceStart`, `sourceLength`, `speed`, `syncGroupId`,
  `linkedAudioClipIds`, `visible`, `muted`, and `videoFxState` (reserved for v2 FX chain
  state). (timeline start/length/offset reuse tracktion's existing clip-position ids.)
- `src/models/CustomClip.h`: extend `enum class Type` with `video`; add `case Type::video:
  return IDs::VIDEOCLIP;` in `clipTypeToXMLType`.
- `src/models/Behavior.h` (`ExtEngineBehaviour`):
  - In `createCustomClipForState`: `if (type == IDs::VIDEOCLIP) return new VideoClip(...)`.
  - In `isCustomClipType`: include `IDs::VIDEOCLIP`.
- `VideoClip` class: derive from **`te::Clip`** (NOT `AudioClipBase`/`WaveAudioClip`/`MidiClip`).
  `AudioClipBase` would make the clip contribute audio nodes — exactly what video must not do.
  `Clip`'s pure-virtual set is small (`isMidi` → false, `canBeAddedTo`, `isMuted`,
  `getDefaultColour`); follow `MarkerClip`/`ChordClip` as the non-audio precedent. Store file +
  source/timeline timing + `syncGroupId` in clip state. Provide static `insertTo(ClipOwner&,
  File videoFile, ClipPosition)` using `CustomClip::insertClipWithState(...,
  CustomClip::Type::video, ...)`, like `PsgClip::insertTo` (`PsgClip.cpp:110-141`). Set clip
  length from the video duration (probe via `juce::VideoComponent` load or platform query; on
  Linux default to a fixed length).
  - **No audio-rendering virtuals to stub** — the payoff of `Clip` over `AudioClipBase`.
- **Embedded video audio = sibling `WaveAudioClip`(s)** (linked-sibling model): on import,
  demux/extract the video's audio to a tracktion-readable source and insert normal
  `WaveAudioClip`(s) on an audio track with the same `syncGroupId`; keep the viewer muted
  (`setAudioVolume(0.0f)`). Audio then flows only through tracktion (latency comp / plugins /
  mixing / export).
  - **CAVEAT (deferred dependency):** demux/extract needs FFmpeg or a platform demuxer
    (AVFoundation/Media Foundation) — the SAME open licensing/packaging decision as the v2
    decode backend (FX addendum). A true minimal v1 can ship picture-only and add A/V
    extraction once that backend is chosen.

### 3. Timeline: `VideoClipComponent`

- `src/gui/timeline/ClipComponents.h` / `.cpp`: add `VideoClipComponent : public ClipComponent`
  that draws the clip block (reuse `ClipComponent::paint` for the rounded rect + selection)
  and overlays the clip name and, optionally, a representative thumbnail frame. Thumbnail is
  a nicety — start with name + a film-strip icon to keep v1 small.
- `src/gui/timeline/TrackComponents.cpp` `buildClips()` (~line 360-371): add a branch
  `else if (dynamic_cast<VideoClip*>(c)) cc = new VideoClipComponent(...)`. Since `VideoClip`
  derives from `Clip` (not `AudioClipBase`), the `WaveAudioClip` branch won't catch it, so
  ordering isn't a hazard — keep it grouped with the other custom-clip checks for clarity.
- Register new `.cpp` files in `src/sources.cmake` (`SHARED_SOURCES`; tests in `TEST_SOURCES`).

### 4. Viewer: `VideoViewerPanel`

New `src/gui/video/VideoViewerPanel.h` / `.cpp`:

- `class VideoViewerPanel : public juce::Component, private te::TransportControl::Listener,
  private juce::Timer` (Timer only active while playing).
- Owns `std::unique_ptr<juce::VideoComponent> video` (constructed with native controls off).
- Registers as a transport listener in ctor (`edit.getTransport().addListener(this)`),
  removes in dtor.
- **Active-clip tracking:** given the current timeline position, find the `VideoClip` whose
  range contains it (iterate the track's clips). When the active clip changes, `load()` the
  new file (synchronous on macOS/Windows) and `setAudioVolume(0.0f)`; when none, clear.
- **`setVideoPosition(TimePosition pos, bool forceJump)` override:** map timeline time → the
  active clip's source time (`pos - clip.start + clip.sourceOffset`, respecting speed). Seek
  (`video->setPlayPosition`) **only when** `forceJump` is set OR the active clip just changed
  OR drift exceeds the threshold. On an ordinary update with the video already playing in
  range, **do nothing** — let `VideoComponent` free-run.
- **`startVideo()` / `stopVideo()` overrides:** `video->play()` / `video->stop()` and
  start/stop the drift-correction timer.
- **DO NOT hard-seek every timer tick.** Calling `setPlayPosition` ~30×/sec fights the
  decoder and causes visible stutter. The timer only *measures* drift and re-seeks when it's
  meaningful; otherwise the video plays itself and stays in sync because it was started from
  the right point. (Key correctness rule for smooth playback.)
- **`timerCallback()` (~30 Hz, playing only):** read latency-compensated time via
  `edit.getCurrentPlaybackContext()->getAudibleTimelineTime()` (fallback to
  `getTransport().getPosition()`), recompute the active clip (switch file if it changed), and
  compare `video->getPlayPosition()` to the expected source time. Re-seek **only** if drift >
  threshold (e.g. > 1–2 frames ≈ 40–80 ms). This bounded correction is where
  **audio-delay-awareness** lives without inducing stutter.
- **Linux / `!MOTOOL_VIDEO_ENABLED`:** compile the `VideoComponent` member out; `paint` an
  "Video playback unavailable on this platform" message; transport overrides become no-ops.

### 5. Wire the viewer into the UI

Per the chosen layout (lane + viewer), the viewer is a panel near the timeline:

- `src/gui/main/MainDocument.h` (studio): add a `VideoViewerPanel` member; in `resized()`
  carve out space for it (e.g. a resizable region beside/below `editComponent_`). Construct
  with `edit` + `evs` like the other components.
- Consider the same for `PlayerDocument.h` if playback-only video is desired there (the
  player already has a side panel pattern — `AYPluginSidePanel`).
- Make visibility toggleable (a `View` menu item / `EditViewState` flag, following the
  existing `showXxx` CachedValue pattern in `EditState.h:108`) so users without video can
  hide it. Optional for v1 but cheap.

### 6. Import / create video clips

- `src/util/Helpers.h` / `.cpp`: add `importVideoAsClip(te::Edit&, te::SelectionManager&,
  bool insertAtCursor)` mirroring `importAudioAsClip` (`Helpers.cpp:137+`): browse for a
  video file (filter `*.mp4;*.mov;*.m4v` on mac, codec-dependent on Windows), get/create an
  audio track via `getSelectedOrInsertAudioTrack`, then `VideoClip::insertTo(*track, file, pos)`.
- Hook into the menu where audio/PSG import lives (search the menu/command setup that calls
  `importAudioAsClip` / `importPsgAsClip` and add a parallel "Import Video…" command).

### 7. Replace raw position with audible time in scroll (small, aligned win)

- `src/controllers/EditState.cpp` `handlePlaybackScrolling()` (~line 154): the existing TODO
  already proposes using `getCurrentPlaybackContext()->getAudibleTimelineTime()`. Adopting it
  makes the playhead, scrolling, and the new video viewer all share the *same*
  latency-compensated clock. Keep the `getTransport().getPosition()` fallback when no context.
  (Do this so video and playhead never disagree.)

---

## Critical files

| Area | File | Change |
|---|---|---|
| Build | `src/CMakeLists.txt` | link `juce_video` + `MOTOOL_VIDEO_ENABLED` (non-Linux) |
| Model ids | `src/models/Ids.h` | `VIDEOCLIP`, `videoSource`, `sourceStart`, `sourceLength`, `speed`, `syncGroupId` |
| Clip factory | `src/models/CustomClip.h` | `Type::video` + xml mapping |
| Clip factory | `src/models/Behavior.h` | `createCustomClipForState` / `isCustomClipType` |
| Model | `src/models/VideoClip.{h,cpp}` (new) | **`te::Clip` subclass** (not `AudioClipBase`) + timing metadata + `insertTo` (mirror `PsgClip.cpp:110`) |
| Timeline block | `src/gui/timeline/ClipComponents.{h,cpp}` | `VideoClipComponent` |
| Timeline dispatch | `src/gui/timeline/TrackComponents.cpp:360` | add `VideoClip` branch |
| Viewer | `src/gui/video/VideoViewerPanel.{h,cpp}` (new) | `VideoComponent` + transport Listener + Timer |
| Host UI | `src/gui/main/MainDocument.h` (and maybe `PlayerDocument.h`) | place viewer |
| Import | `src/util/Helpers.{h,cpp}` | `importVideoAsClip` (mirror `importAudioAsClip`) |
| Sync clock | `src/controllers/EditState.cpp:154` | use `getAudibleTimelineTime()` |
| Sources | `src/sources.cmake` | register new `.cpp` (+ `.test.cpp`) |

### Reused existing patterns
- Custom clip plumbing: `CustomClip::insertClipWithState` (`CustomClip.h:21`) and
  `PsgClip::insertTo` (`PsgClip.cpp:110`).
- Clip→component dispatch: `TrackBodyComponent::buildClips` (`TrackComponents.cpp:360`).
- Transport listener + audible time: `TransportControl::Listener`
  (`tracktion_TransportControl.h:335`), `EditPlaybackContext::getAudibleTimelineTime/
  getLatencySamples`.
- Timeline coords: `ZoomViewState::timeToX / xToTime` (`EditState.h:73`).
- Import flow: `importAudioAsClip` (`Helpers.cpp:137`).

---

## Risks & notes

- **`Clip` base virtuals:** small set (`isMidi`, `canBeAddedTo`, `isMuted`,
  `getDefaultColour`). Confirm during impl by reading
  `third_party/tracktion_engine/.../model/clips/tracktion_Clip.h` and cross-checking
  `MarkerClip`/`ChordClip` as the non-audio precedent. No audio-rendering virtuals to stub.
- **Video audio routing:** via linked-sibling `WaveAudioClip`(s) inside tracktion (NOT the
  `VideoComponent`, kept muted). Needs a demux/extract step — see the decode-backend open
  decision in the FX addendum; until chosen, v1 is picture-only and audio-linking is a
  fast-follow.
- **Windows codecs:** playback depends on installed codecs (DirectShow/MF). Document; not a
  code issue.
- **`juce::VideoComponent::load` is synchronous on mac/Windows** — fine for clip switches,
  but a very large file could hitch the message thread. If noticeable, switch to `loadAsync`.
- **Drift threshold tuning:** start ~40 ms; expose as a constant.

---

## Verification

1. **Build all three platforms locally / CI:** `cmake -S . -B build && cmake --build build`.
   Confirm Linux builds with video compiled out (no `juce_video` link, placeholder viewer).
2. **macOS manual test:** run studio
   (`build/src/uZX_artefacts/Debug/μZX.app/.../μZX`), Import Video…, confirm a clip block
   appears on the lane and the viewer shows the first frame.
3. **Sync test:** place a video clip alongside a PSG/wave clip; press play; visually confirm
   the frame tracks the playhead and the viewer starts/stops with transport. Scrub the
   playhead (drag) and confirm the frame follows (`setVideoPosition` forceJump path).
4. **Multi-clip test:** add 2–3 video clips with gaps; play across boundaries; confirm the
   viewer swaps source files and shows blank in gaps.
5. **Latency test:** introduce output latency (larger audio buffer in device settings) and
   confirm the displayed frame matches the *audible* music (audible-time path), not running
   ahead by the buffer size.
6. **Unit test:** `src/models/VideoClip.test.cpp` — create an Edit, `VideoClip::insertTo`,
   reload the Edit ValueTree, assert the clip round-trips with its file path and is
   reconstructed as a `VideoClip` via `createCustomClipForState`. Run with
   `build/.../uZXTests VideoClip`.

---

# Addendum: Video FX chains — architecture decision

## Question

Should video FX (color correction, blur, overlays, transitions, etc.) be implemented by
**extending tracktion's audio graph** with video nodes, or by building a **parallel video
graph** alongside it?

## Decision: build a parallel video graph. Do NOT extend tracktion's audio graph.

### Why the audio graph cannot host video

Tracktion's processing graph is a hard-typed **audio + MIDI** graph. Verified in
`third_party/tracktion_engine/modules/tracktion_graph/tracktion_graph/tracktion_Node.h`:

- `Node::process(ProcessContext&)` only ever receives an
  `AudioAndMidiBuffer { choc::buffer::ChannelArrayView<float> audio; MidiBuffer midi; }`
  (Node.h:246-255, 309-313). There is no frame / texture / pixel concept anywhere in the
  context.
- `NodeProperties` is `{ hasAudio, hasMidi, numberOfChannels, latencyNumSamples, nodeID }`
  (Node.h:152) — no video dimension.
- The graph runs on the **real-time audio thread**, at the audio block rate (e.g. ~512
  samples ≈ 10 ms), driven by the audio device callback.

A "video node" inside this graph would carry no video data, run on the wrong thread, at the
wrong rate. Video FX (decode, GPU passes, color ops) belong on the message/GPU thread at
~24–60 fps and may take tens of ms — running them on the audio thread would cause dropouts.
`ProcessContext` / `NodeProperties` are the only extension seams, and both are audio-only,
so this would be a deep fork of tracktion, not an extension.

### The parallel video graph

Tracktion stays the **authoritative clock and audio engine**. A separate, pull-based video
pipeline is *driven by* the engine clock but not *contained* in it:

```
tracktion audio graph ──► authoritative clock (EditPlaybackContext::getAudibleTimelineTime,
        │                                       latency-compensated)
        │ pull: "give me the composited frame at timeline time T"
        ▼
Video graph (our code; message/GPU thread; frame rate)
   active VideoClip(s) → decode frame at T
        → FX chain: [ColorCorrect] → [Blur] → [Overlay] → ...   (ordered, per clip/track)
        → composite → viewer
```

Properties:
- **Pull-based & time-addressed.** The video side asks for the frame at timeline time `T`,
  where `T` is tracktion's latency-compensated `getAudibleTimelineTime()` — the *same* clock
  the v1 viewer and the playhead use. The FX chain simply sits between decode and display, so
  **sync stays solved** and audio-delay-awareness is inherited for free.
- **Off the audio thread.** No real-time constraint; FX can be as heavy as the GPU allows. A
  late frame just repeats the previous one (graceful degradation — unlike audio).
- **FX as an ordered list**, each FX a pure `frame_in → frame_out`. This mirrors tracktion's
  own `pluginList` (an ordered chain compiled into a processing graph) conceptually — same
  mental model, different data type (pixels, not samples).

### Will it work? Yes — with one real caveat about the decode layer

The architecture is sound and keeps sync trivial. The caveat: **`juce::VideoComponent` is a
black box** — it decodes *and* renders to its own native view (AVPlayer / DirectShow). You
cannot grab a decoded frame, run an effect on it, and re-composite. So a true FX chain
requires owning the decode layer (decode to images/textures yourself), which the v1 viewer
does not do. This is why the work splits into two phases.

## Two-phase plan

### v1 — sync, no FX (as in the main plan above)
- `juce::VideoComponent` for playback, driven by `TransportControl::Listener` +
  `getAudibleTimelineTime()`. Ships synced video in the viewer. **No FX** (black-box
  renderer). This is the entire main plan above; nothing changes.

### v2 — FX-capable compositor (swap the decode layer)
- Replace `VideoComponent` in `VideoViewerPanel` with a **frame-grabbing decoder +
  compositor**:
  - **Decoder**: per `VideoClip`, decode the frame nearest timeline time `T` into an
    in-memory frame (CPU image or GPU texture).
  - **FX chain**: an ordered list of effects applied to the frame. Model it after the
    `pluginList` pattern; persist FX + params in the clip/track ValueTree (same custom-state
    approach already used for clips).
  - **Compositor**: combine layers/clips and the FX output into the final frame, drawn into
    the viewer.
- The clock, clip model (`VideoClip`), timeline lane (`VideoClipComponent`), and viewer
  *shell* (`VideoViewerPanel`) from v1 are all **reused unchanged**; only the inside of the
  viewer (decode → FX → composite) is swapped. The v1→v2 transition is therefore contained.

## OPEN DECISION — decode + render/compositor backend (NOT decided)

Deferred until v2 work starts. **Note: whether to use OpenGL at all for the compositor is
itself undecided** — the FX chain could be CPU-side (`juce::Image` / JUCE `Graphics`) for
simple effects, or GPU-side (OpenGL/Metal shader passes) for performance and richer effects.
Pick when v2 begins. Candidate combinations and tradeoffs:

| Option | Decode | FX / compositor | Pros | Cons |
|---|---|---|---|---|
| Platform-native + GPU | AVFoundation (mac) / Media Foundation (Win) → texture | OpenGL/Metal passes via JUCE `OpenGLContext` | No new third-party binary; uses existing JUCE/platform approach; fast | Per-platform decode code; **Linux has no decoder → FX disabled on Linux**; GPU pipeline complexity |
| FFmpeg + GPU | FFmpeg (libav*) → frame → texture | OpenGL/Metal passes | Single cross-platform decoder incl. **Linux**; widest format support; uniform code | Adds FFmpeg dependency; **LGPL/GPL licensing** + binary-size/build/packaging implications |
| CPU-only FX | (either decoder above) | `juce::Image` + JUCE `Graphics` on message thread | Simplest; no GL pipeline; trivially cross-platform for the FX part | Limited to lightweight effects; slower for HD/many effects/realtime |

Factors to weigh when deciding:
- **GL vs CPU**: depends on the effect set (simple overlays/fades → CPU may suffice;
  blur/transforms/grading at HD/realtime → GPU). Currently undecided.
- **Linux**: only FFmpeg gives Linux decode; native path leaves Linux without video (consistent
  with v1, which already compiles video out on Linux).
- **Licensing/packaging**: FFmpeg changes the project's dependency and license story; the
  native path does not.

No backend is selected here by request; this section documents the choice space only.
