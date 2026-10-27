# DEVLOG

Per-session journal and the sole home for superseded decisions (the live tree
describes only the current design). Newest on top; one block per session, keyed
by `session_id`. Keep entries terse. Did: 300 chars, Difficulty, Model, Agent, Friction: one line.

<!-- entries below -->

## 2026-07-01 -- Implement latency-aware playhead sync (`0ca5678b-6880-4769-b32f-9b9b3e224ecf`)

- **Did:** Built `getAudibleTimelinePosition()` helper to centralize playhead/scroll sync logic: prefers audible time from playback context while playing/recording, falls back to raw transport when stopped (avoids stale audible time after seek). Applied to `PlayheadComponent` and `EditState::handlePlaybackScrolling()`. Added regression test for seek-to-Home behavior. Commit 09e9f51.
- **Difficulty:** 2 -- clear task; main challenge was understanding when audible time stales after seek.
- **Model:** claude-opus-4-8 -- fit.
- **Agent:** good -- methodical exploration of Tracktion APIs and playback context lifecycle; caught the stopped-seek regression before shipping.
- **Friction:** some -- first fix assumed audible time was always current (broke Home seek); user feedback + reasoning led to the stopped state check.

## 2026-07-02 -- Snap playhead to visible grid (`fa6ac6eb-2222-4e9a-89fd-6c0ca8d7a747`)

- **Did:** Added default-on playhead snapping to the nearest visible TimelineGrid level, reusing SnapLadder's displayed fine level instead of Tracktion's native snap type. Stored the option in `EditViewState`, exposed it in the View menu and ruler context menu, and applied it to playhead drag plus ruler/clip/track timeline seeks. Added TimelineGrid regression coverage and put the test in `TEST_SOURCES`.
- **Difficulty:** 2 -- clear integration points; main risk was keeping snap behavior identical to the drawn grid and covering all timeline seek paths.
- **Model:** gpt-5 -- fit.
- **Agent:** good -- subagents quickly mapped state/menu and grid/Tracktion seams; implementation stayed local and tests caught source membership.
- **Friction:** low -- initial default was off, then corrected to default-on after user screenshot.

## 2026-07-02 -- Timeline grid redesign: SnapLadder replaces TimecodeDisplayFormatExt (`5a9e379f-56e8-4a66-922f-14e2b16039e3`)

- **Did:** Replaced the three divergent grid tick paths with one Tracktion-style level ladder (`src/models/SnapLadder.*`): integer frame-in-beat rounding, swing subdivision level (e.g. 6-7-6-7 at FPB 26, stored in `IDs::gridSubdivision`), single generator in TimelineGrid. Dropped `TimecodeTypeExt`/`TimecodeDisplayFormatExt`; mode-only `IDs::timecodeDisplayMode` + legacy-string migration on read.
- **Difficulty:** 3 -- design-heavy but well-scoped; one real bug (infinite recursion at beat-boundary float round-trip) only surfaced by launching the app, not by tests.
- **Model:** claude-fable-5 -- fit
- **Agent:** good -- subagent handled 10-file mechanical call-site batch cleanly, all suites green first run
- **Friction:** low -- plan approved with an execution-strategy note (subagents for simple work)
- **Superseded:** `TimecodeDisplayFormatExt`'s 19-value enum encoding (mode × fps) and `getOptimalSnapTypes` table — fps was already edit-global; also MainController no longer stomps the display mode on every edit load (was resetting to barsBeatsFps50), it now only defaults when unset.

## 2026-07-02 -- Found timeline zoom level constraints (`f251b997-e9a0-4979-84bd-2a66c53ad8a0`)

- **Did:** Located min/max timeline zoom level definitions (0.0005s to 2s per pixel) in EditState.cpp via grep and code review.
- **Difficulty:** 1 -- simple lookup, clear code structure
- **Model:** claude-haiku-4-5 -- fit
- **Agent:** good -- direct search strategy, found target quickly
- **Friction:** none

## 2026-07-02 -- Fix playhead vanishing during fast drags (`e69bd2ac-155e-410f-8927-d2d203fd4bbb`)

- **Did:** Fixed the v0.9 P0 "playhead disappears while dragging fast". Root cause: JUCE's macOS peer flushes deferred repaint rects at vblank, and mouse events landing between flush and drawRect move `xPosition` past the stale dirty region, so the line is clipped out on nearly every frame at speed. Fix: `PlayheadComponent::checkRepaint` invalidates one rect padded by 2× the current step on both sides, while bounding the invalidated area.
- **Difficulty:** 3 -- two plausible static theories were wrong (span-repaint cost, paint starvation); only instrumented live-run data exposed the flush/draw race.
- **Model:** claude-fable-5 -- fit; the clip-vs-x log analysis was the decisive step.
- **Agent:** good -- switched from theorizing to measurement after the first fix failed; verified fix with the same metric (0/352 clipped).
- **Friction:** some -- first fix shipped on an unverified assumption and user had to bounce it back; verification loop needed user driving the GUI.

## 2026-07-01 -- Fix MIDI device rescan stopping playback (`6a31b912-fea1-44a6-a073-9d80dade3d40`)

- **Did:** Fixed playback stopping 1-3s after transport start: MIDI device rescans (startup + hardware changes) interrupted the transport. Two-part fix: tracktion fork defers device-list updates while playing (retry every 500ms); uZX Player disables periodic MIDI scanning entirely. Commit 06628b8 + submodule patch.
- **Difficulty:** 3 -- traced async device-rescan vs transport lifecycle across the engine.
- **Model:** claude-fable-5 -- fit.
- **Agent:** good -- Explore agents mapped the engine accurately; mechanism understood before fixing.
- **Friction:** low -- clear symptom, collaborative decisions.

## 2026-07-01 -- PSG painting refactored (`f798fbdc-0fb1-430f-8074-378ef8bdaae7`)

- **Did:** Fixed PSG clip painting to source frame duration from the PSG clip itself rather than the edit; refactored pitch range calculation and note painter to use shared enumeration; added hearable-window filtering for envelope notes. Commit 8cccbcb.
- **Difficulty:** 2 -- straightforward refactoring with clear ownership.
- **Model:** claude-fable-5 (sonnet for review) -- fit.
- **Agent:** good -- systematic, incremental, tested.
- **Friction:** some -- filtering logic needed user clarification mid-session.

## 2026-07-01 -- Review and fix PSG integer-timing cherry-pick (`4633044f-c270-4012-8d24-58e678dfd088`)

- **Did:** Reviewed commit c7895fc (integer frame timing for PSG clips) against develop, found cherry-pick artifacts (orphaned `PsgTimingPreserver` files, dropped `ProjectFps.test.cpp`, lost drift tests) and fixed them: deleted dead files, restored the test to the build, hardened migration rate selection, clamped frame indices, ported invariant tests.
- **Difficulty:** 3 -- complex model review; fixes straightforward but careful test restoration.
- **Model:** claude-fable-5 -- fit.
- **Agent:** good -- systematic review of design edges; fixes applied cleanly.
- **Friction:** low -- one follow-up to defer multi-tempo work.

## 2026-07-01 -- PSG integer frame timing with frames-per-beat (`be9fb430-bea4-4b0a-906a-49f2eabebb69`)

- **Did:** Rearchitected PSG timing (on feature branch `integer-psg-sketch`, later cherry-picked to `integer-psg-on-develop`): integer machine-frame index `IDs::i` becomes the sole stored timestamp; beats derived via list-level `framesPerBeat`; `PsgFrameRetimer` replaces snapshot-based preserve by scaling metadata only; legacy beat-stored files migrated on load.
- **Difficulty:** 4 -- timing math, `IDs` namespace collision between `EditState.h` and `Ids.h`, JUCE Label editor-dismissal race, large test rewrite.
- **Model:** Opus 4.8 -- fit.
- **Agent:** good -- systematic exploration; caught the callback-timing bug; clean test refactor.
- **Friction:** some -- include-chain fixes for the namespace collision; unintuitive Label `onEditorHide` ordering.
- **Superseded:** per-frame `te::IDs::b` beat storage and `PsgTimingPreserver` snapshot/remap replaced by frame-index + `framesPerBeat` metadata scaling.

## 2026-07-01 -- Rescale-aware PSG fps warning; geometry model (`a25a9075-354e-47fc-9139-b6e9052a5431`)

- **Did:** Added a per-clip fps-mismatch warning chip to `PsgClipComponent` (single header row: legend + name + amber `⚠ N @ M fps`), then iterated the underlying model through four designs until landing on pure geometry: `effectiveFps = framesPerBeat × currentBpm / 60`, `framesPerBeat = round(1/smallestBeatGap)` — nothing stored on the clip, so no drift, no migration, legacy clips just get measured. Non-preserving BPM change rescales it, preserve holds it (frame beats rewritten). Also fixed the quantisation round-trip + duplicate-visit bugs in `PsgTimingPreserver`, extracted that machinery out of `EditViewState`, and flipped preserve-timing to default-on. Commits `a457301`, `f5cac9b`.
- **Difficulty:** 3 -- the model itself is simple; getting there took real reasoning about what's invariant under tempo/preserve/rescale.
- **Model:** Opus 4.8 -- fit.
- **Agent:** good -- caught its own float-drift and repaint-trigger bugs; verified each design against the "preserve keeps effective fps constant" criterion via tests + live app runs.
- **Friction:** some -- several user-driven course corrections (drop frame indices, store anchor not stretch, then store nothing, remove sourceFps); each was a genuine simplification, not a mistake.
- **Superseded:** stored `sourceFps`/`anchorBpm`/`stretchFactor` and the frame-index experiment all removed in favour of geometry-derived fps; the tempo-derived `getBaseFrameBeatGap` effective-fps (float-drift `51@51`) replaced by integer `framesPerBeat`.

## 2026-06-30 -- Plan video clips/tracks + FX-chain architecture (`ec3f4ab5-1743-452c-8d13-ba2fd637c683`)

- **Did:** Design-only. Wrote `docs/video-proposal.md`: `VideoClip` as a custom `te::Clip` (via the existing `PsgClip` clip-factory hook) on plain `AudioTrack`s, viewer driven by transport callbacks + audible time; FX go in a parallel video graph, not tracktion's audio-only graph. Reconciled an external codex draft — adopted `te::Clip` (not `AudioClipBase`) and the linked-sibling audio model; kept claude doc canonical, removed codex doc.
- **Difficulty:** 3 -- no code, but needed deep tracktion tracing to pin the right extension seam.
- **Model:** Opus 4.8 -- fit.
- **Agent:** good -- verified claims against source; caught own earlier `AudioClipBase` error.
- **Friction:** some -- AskUserQuestion `preview: null` rejections; plan-mode edit re-routing.

## 2026-06-29 -- Fix playhead freeze from incompatible audio device (`b8774244-6aad-4853-81d2-ab29447a4223`)

- **Did:** Diagnosed Studio playhead stuck at a fixed pos + unrepositionable (Player unaffected). Traced through tracktion: `TransportControl::timerCallback` keeps writing `EditPlaybackContext::audiblePlaybackTime` back into transport `position`; ~200ms snap-back is `lastUserDragTime` suppression. Real root cause (via user testing + `system_profiler`): macOS CoreAudio runs a separate input device (≠ output, e.g. BlackHole-in + UR22-out) as an aggregate whose callback never drives the graph, freezing audible time. Built `AudioSettingsComponent` (wraps JUCE selector, rejects input≠output by reverting to last valid + alert, auto-fits dialog height with pinned width), `sanitizeAudioInputDevice()` at startup, and a Play guard backstop. Committed `7e6fac5` to `develop`.
- **Difficulty:** 4 -- deep engine spelunking; root cause was environmental and only surfaced through iterative user repro, not static analysis.
- **Model:** Opus 4.8 -- fit.
- **Agent:** mixed -- correct mechanism early, but proposed several wrong fixes first (minInputChannels=1 just hides the checkbox; auto-enabling a channel triggered the per-block `tracktion_WaveInputDevice:1183` assertion). User corrections steered to the working block-and-revert design.
- **Friction:** high -- multiple iterations on the dialog UX (flashing warning, empty space, collapsed width) and on the fix strategy; couldn't drive the GUI so relied on user screenshots + a DBG-logging run to confirm sizing.
- **Superseded:** dropped the "different devices" warning-string check and the channel-forcing auto-fix and the device-change one-shot warning (`audioDeviceWasBroken_`); all replaced by source-level prevention. `minInputChannels` left at 0.

## 2026-06-29 -- Finish PSG initial-state-at-t=0 bugfix (`7cdbd1fb-5b49-401c-832c-b27ab0438b3c`)

- **Did:** Resumed half-done fix in `PsgList::exportToPlaybackMidiSequence`. PSG frames only mask changed registers, so playhead reposition inherited stale AY state. Forced all masks on for frame 0 to emit a full register snapshot (works because `recomputeAccumulatedState()` already copies full accumulated `values` into every frame). Finalized + strengthened the `PsgListInitState` test (verifies full-snapshot emission and VolumeA=10 value; fixed `-Wfloat-equal`). Verified all 22 param types are handled by `PsgParamsMidiWriter::write` so forced masks won't hit the `jassertfalse` default. Committed + pushed to `develop`.
- **Difficulty:** 2 -- mostly understanding the existing accumulation model; the fix itself was small.
- **Model:** Opus 4.8 -- fit.
- **Agent:** good -- found the accumulated-state mechanism and the writer's exhaustiveness check before committing to the approach.
- **Friction:** low -- one stale-file-read retry on an edit; left unrelated working-tree changes (ROADMAP, CMakeLists, user-guide.md) uncommitted as intended.

## 2026-06-29 -- Debug VSCode launch configuration issue (`e617694b-a27b-4f0c-97b1-8021b3bcee24`)

- **Did:** Diagnosed and resolved VSCode launch failure on macOS. Root cause was an expired code signing identity (Y76AVTDMZ2) that caused the CMake build to fail silently. Guided user to disable code signing via CMake reconfiguration (`unset CODESIGN_IDENTITY && cmake`) and gave steps to restore identity via Xcode Accounts settings. No code changes.
- **Difficulty:** 2 -- straightforward toolchain issue; required inspecting VS Code config (launch.json, CMake Tools settings), cmake output, and keychain state.
- **Model:** claude-sonnet-4-6 -- fit; competent at config debugging and tool-output parsing.
- **Agent:** good -- methodically traced symptom (launch does nothing) → empty app bundle → build failure → codesign identity error; gave a quick workaround plus the proper fix.
- **Friction:** low -- user provided a screenshot and build output; minimal back-and-forth.

<!-- Below: older sessions backfilled from MoTool-era transcripts (project was named MoTool until early 2026; transcripts restored from macbook_backup). -->

## 2025-12-27 -- Implement PsgList data model enhancements (`e1e945a0-b421-4d99-a640-ecb36a362d8b`)

- **Did:** Expanded `PsgList` model — parameter storage, query methods, GUI integration. Added `sources.cmake` entry, ~9 edits across core model and timeline components, refactored `PsgClipComponent` parameter binding, extended test coverage and type safety.
- **Difficulty:** 2 -- clear-scope data model work.
- **Model:** claude-opus-4-5 + claude-sonnet-4-5 (switched mid-session) -- fit.
- **Agent:** good -- effective Glob exploration, incremental edits, test-first approach.
- **Friction:** none.

## 2025-12-27 -- Fix EditPosition API migration in CurveEditor (`54e648e5-de1f-4d06-87dd-dc4f9dd34bf9`)

- **Did:** Updated `PsgParamEditorComponent` for tracktion_engine API change (TimePosition → EditPosition in CurveEditor methods). ~17 mechanical edits across .h/.cpp to fix compile errors from the upstream shift.
- **Difficulty:** 2 -- high-volume but mechanical rename.
- **Model:** claude-sonnet-4-5 -- fit.
- **Agent:** good -- Grep-based discovery, consistent application of the pattern.
- **Friction:** low.

## 2025-12-26 -- Resolve git rebase submodule conflict (`9d5aa613-5cea-4f92-88d7-6c3c9ecf8998`)

- **Did:** Resolved a rebase conflict in the tracktion_engine submodule's nested juce sub-submodule during an interactive rebase; reset the conflicting submodule to base to continue.
- **Difficulty:** 4 -- nested submodule state mid-rebase.
- **Model:** claude-sonnet-4-5 -- overshoot.
- **Agent:** mixed -- correct diagnosis, but struggled with bash cwd not persisting across calls; needed workarounds.
- **Friction:** high -- repeated `cd`-context failures before grasping the structure.

## 2025-12-26 -- Fix macOS build after OS update (`172f1b1e-5593-4cc7-a73d-1d6e4fcfc688`)

- **Did:** Fixed a missing `TargetConditionals.h` compile error from a stale CMake cache after an OS update, then a deeper AppleClang 17 incompatibility in tracktion_engine (`override` on a non-virtual method); patched 3 files in the audio format module.
- **Difficulty:** 3 -- cache cleanup was easy but exposed a third-party compiler-compat issue.
- **Model:** claude-sonnet-4-5 -- fit.
- **Agent:** good -- systematic diagnosis to root cause with a working fix.
- **Friction:** some -- required navigating the nested submodule structure.

<!-- trivial sessions (no summary) -->
06becf55-c1b2-4e28-9cff-8b25fa630360
4660af07-b97d-4292-b54c-6405e49d7ebc
agent-a72a3f1
agent-aac0079
cc76f453-782e-48f0-81d8-cafd776c1f1e
110f7244-15b3-466c-823d-9222acf0543d
15c7455a-38e3-45ee-8b6f-b94a29e97ac1
