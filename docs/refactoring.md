# Refactoring plan

Review started: 2026-11-09. Bounded source-inspection passes cover controller lifecycle and commands, persistence, PSG import and serialization, timeline components, derived-state caches, parameter bindings, tuning export and preview, device UI integration, track rendering, scope publication/settings, and selected audio configuration/instrument paths. Findings distinguish confirmed code structure and result-handling errors from runtime risks that still need reproduction. Builds, tests, native UI checks, and audio contention measurements have not been run during the review. This is not an exhaustive repository audit.

Track completion in [TODO.md](TODO.md#refactoring). Keep each step small and behavior-preserving: establish the ownership boundary first, then make any behavior changes separately. Do not introduce a general framework for these extractions.

## Implementation order

1. **Finding 2:** consolidate AY plugin setup, the smallest duplicated operation.
2. **Finding 4:** move timecode menu construction out of model utilities.
3. **Finding 1:** consolidate edit replacement after establishing lifecycle coverage.
4. **Finding 3:** inspect the inherited parameter-editor behavior before replacing its base class.

Review and validate one step at a time. Use focused tests for the changed behavior and native UI checks where needed; do not repeat a repository-wide audit for each step. If a step changes anything users can see or do, update `user-guide.md` in the same commit.

The next review priorities are **5** (persistence, coordinated with 1), **6** (audio ownership), **7** (plugin clock coordination), and **8** (import timing). These require checking behavior before settling the implementation. **9** (export formatting) is a lower-priority extraction. The small refactors above remain useful independent steps; they do not replace the correctness reviews.

The third pass adds **10–11** (PSG mutation and cache dependencies), **12** (widget callback lifetime), and **13** (duplicated persisted parameter mapping). Review 10 and 11 together, with separate regression cases. The small mapping extraction in 13 can precede changes to frame-state reconstruction in 10. These concrete ownership issues take precedence over the cosmetic benefit of splitting large files.

The fourth pass adds **14** (deferred preview requests), **15** (render rollback), **16** (scope menu ownership), and **17** (MIDI sample-range contracts). Prioritize failure/cancellation behavior in 14–15; 16 is an independent small extraction. For 17, establish supported nonzero-start callers before broadening the processing contract.

The fifth pass adds **18** (command capabilities), **19** (scope sample publication), and **20** (duplicated scope settings binding). Review 19 alongside the other audio ownership work in 6: it identifies a concrete unsynchronized sample-storage path. Findings 16 and 20 can share a small scope-UI cleanup, while the publication fix in 19 should have separate concurrency validation.

## 1. Centralize edit replacement lifecycle

**Evidence:** `ArrangerController::setEdit` in [MainController.cpp](../src/controllers/MainController.cpp) and `PlayerController::setEdit` in [PlayerController.cpp](../src/controllers/PlayerController.cpp) repeat the same ordered lifecycle: optionally save the previous edit, clear window content and selection, release view/playback state, remove the old temporary directory, install the new edit, and rebuild playback and view state.

**Problem:** the ordering protects references to the old edit, but two implementations own that knowledge. A lifetime fix can reach one app and leave the other unchanged. The existing `BaseController` already owns the edit, window, playback helper, and view state.

**Proposed boundary:** move shared teardown and installation into the existing `BaseController`. Keep Studio and Player policy explicit at the call sites: timecode migration versus seconds display, document type, headers, and window title. Preserve current ordering and error handling; avoid a new inheritance hierarchy or a generic callback framework.

**Validation:** cover null replacement retaining the current edit, dependent objects being released before the old edit, and successful replacement in both apps. Existing `PlayWholeEdit` and `EditState` tests cover related pieces, but do not establish the complete controller lifecycle. Exercise opening and switching projects in Studio and Player with the native UX harness.

**Related issue:** the Global TODO already records data loss when reopening the current project. Treat that as a separate behavior fix with its own regression case; consolidation alone does not resolve it.

## 2. Consolidate AY plugin setup during PSG import

**Evidence:** [PlayerController.cpp](../src/controllers/PlayerController.cpp) contains both `ensureAYPluginOnTrack` and a separate scan/create/insert sequence in `importPsgToNewEdit`. [Helpers.cpp](../src/util/Helpers.cpp), in `importPsgAsClip`, contains a third implementation.

**Problem:** the Player helper implicitly uses `edit_` for plugin creation, so it cannot serve the newly prepared edit before that edit becomes current. This hidden dependency causes duplication of the rule that a PSG import must have an AY plugin on its destination track.

**Proposed boundary:** extract one track-level operation that ensures the plugin exists and uses the track's owning edit for its plugin cache. Route all three implementations through it. Keep file dialogs, validation messages, transport decisions, track selection, cursor placement, naming, and zooming in their current workflows.

**Validation:** check that a track without an AY plugin receives one, an existing AY plugin is retained without duplication, and a track belonging to a newly prepared edit uses that edit's cache. Existing PSG clip insertion tests do not cover plugin assignment. Smoke-test Studio import and both Player import destinations.

## 3. Remove the parameter display's unsupported editing contract

**Evidence:** [PsgParamEditorComponent.h](../src/gui/timeline/PsgParamEditorComponent.h) derives from Tracktion `CurveEditor`. In [PsgParamEditorComponent.cpp](../src/gui/timeline/PsgParamEditorComponent.cpp), `removePoint`, `addPoint`, `movePoint`, `createPoint`, and several Bézier methods are empty or return placeholder values.

**Problem:** the component exposes editing operations it does not implement and carries an unrelated API surface to support its current display behavior. This is an abstraction mismatch; whether inherited mouse or keyboard interactions produce a visible failure was not established during the review.

**Proposed boundary:** first inspect Tracktion's `CurveEditor` implementation and identify the selection listeners, coordinate conversion, tooltips, zoom, and input behavior the component actually uses. Then replace the unsupported editing inheritance with a display component, retaining only the needed behavior. Do not implement PSG editing as part of this refactor; that is a separate feature in TODO.

**Validation:** establish focused coverage for no clip, selected clip, clip deletion, parameter switching, zoom, and selection updates. Check mouse and keyboard behavior with the native UX harness. Existing `PsgClipComponent` rendering tests exercise a different component and are not sufficient for this change.

## 4. Move timecode menu construction into the GUI layer

**Evidence:** `buildTimecodeFormatMenu` in [EditUtilities.cpp](../src/models/EditUtilities.cpp), declared in [EditUtilities.h](../src/models/EditUtilities.h), constructs a `PopupMenu`, labels, checked states, and callbacks alongside persisted timecode settings and migration logic. Callers exist in both [Ruler.cpp](../src/gui/timeline/Ruler.cpp) and [MainController.cpp](../src/controllers/MainController.cpp).

**Problem:** model utilities own UI presentation as well as edit state. Menu changes therefore touch a model implementation used by PSG and timing code.

**Proposed boundary:** move menu construction and its declaration into a shared GUI helper, using the existing GUI utilities if appropriate. Keep timecode accessors, persistence, and legacy migration in `models/EditUtilities`. Update all ruler and main-menu callers; do not duplicate the menu at each call site.

**Validation:** retain menu labels, order, checked states, and callback behavior. Verify ruler and main-menu selections update the display mode without changing project FPS. Run the existing `ProjectFps` tests for the state/migration behavior and check both menu entry points in the UI.

## 5. Separate persistence results and recent-file ownership

**Evidence:** [FileOps.cpp](../src/util/FileOps.cpp) combines save operations, file dialogs, application identity, and recent-file storage. `saveEditAsWithDialog` ignores the boolean returned by `saveEditAs` and returns true after a file is selected. `addToRecentEdits` inserts newest entries at index zero, but `getRecentEdits` trims lists longer than eight by removing index zero. Storage is resolved through `Engine::getEngines()[0]`; controllers also clear the `recentEdits` property directly.

**Problem:** a failed save is reported to the caller as success, and an over-limit recent-file list drops its newest entries. These result/order errors follow directly from the code; disk-failure and native-dialog scenarios have not been exercised. Global storage lookup also hides which engine owns the operation and complicates isolated tests.

**Proposed boundary:** separate choosing a destination from saving it, preserve the save result, and give recent-file operations an explicit storage dependency. Centralize recent-file ordering, limits, and clearing. Establish the boundary before correcting behavior; do not build a general persistence framework.

**Validation:** test save failure and cancellation, newest-first ordering beyond eight entries, duplicates, missing files, and clearing the same storage used for reads. No direct FileOps tests were found. Coordinate with finding 1 and the existing same-project reopen issue; those involve additional edit lifetime and save-order behavior.

## 6. Establish audio-thread ownership of mutable runtime state

**Evidence:** [AYPlugin.cpp](../src/plugins/uZX/aychip/AYPlugin.cpp), in `applyToBuffer`, holds a blocking lock across event processing and rendering. `reset`, configuration callbacks, and register reads use the same lock. The MIDI-to-PSG mapper's unsynchronized configuration changes are already recorded in TODO.

**Problem:** UI/configuration work can contend with the render callback. The lock scope is confirmed, but audible glitches and their frequency have not been measured. Removing locks without a replacement ownership contract would introduce races.

**Review boundary:** trace every writer and reader of emulator/mapper state, including startup, reset, panic, configuration, diagnostics, and shutdown. Evaluate applying pending configuration at audio-block boundaries so one thread owns mutable processing state. Account for object allocation and destruction as well as field writes; do not assume an atomic pointer alone solves real-time ownership.

**Validation:** inspect `AYPlugin.test.cpp`, `AYChip.test.cpp`, and `NotesToPsgMapper.test.cpp`; cover configuration and reset while processing, note silencing, and shutdown. Measure callback contention under repeated UI changes. This extends the existing mapper race task rather than duplicating it as a new discovery.

## 7. Make clock coordination between plugins explicit

**Evidence:** `NotesToPsgPlugin::setTuningSystem` in [NotesToPsgPlugin.cpp](../src/plugins/uZX/notes_to_psg/NotesToPsgPlugin.cpp) searches for a downstream AY plugin and writes its `staticParams.chipClock`. The AY plugin independently owns and persists that setting in [AYPlugin.h](../src/plugins/uZX/aychip/AYPlugin.h).

**Problem:** configuring the converter mutates another plugin's persisted state. The effective clock policy is split between plugins and depends on chain presence/order. The source already hints at this dependency problem; whether the current mutation is the desired product behavior still needs checking.

**Review boundary:** define who owns the effective clock for a converter/synth pair. Move coordination to the owning track or plugin-chain boundary, preserving current behavior first. Avoid a converter reaching through another plugin's parameter internals; do not invent a general plugin messaging system.

**Validation:** test absent, present, reordered, and multiple AY plugins; clock changes from each UI; and save/reload. Mapper and AY unit tests cover individual parts, but a chain-level case is needed. Coordinate the update mechanism with finding 6.

## 8. Separate PSG metadata, clip timing, and project policy

**Evidence:** [PsgFile.h](../src/formats/psg/PsgFile.h) parses header interrupt rate but does not transfer it to [PsgData](../src/formats/psg/PsgData.h), whose frame rate defaults to 50. `PsgList::loadFrom` in [PsgList.cpp](../src/models/PsgList.cpp) sets project-wide FPS and derives frames per beat at time zero. The project-FPS side effect and time-zero mismatch are already in TODO.

**Problem:** file decoding, clip-local timing, and project settings lack a clear ownership boundary. Header metadata can be lost before import, while importing one clip changes an edit-wide setting. The exact handling of supported file versions and non-default interrupt rates needs fixture-based verification before changing defaults.

**Review boundary:** trace header metadata into decoded data, clip timing, and explicit import policy. Keep parsing responsible for source metadata, clip conversion responsible for its supplied timing context, and the import caller responsible for project-setting decisions. Preserve legacy migration behavior separately.

**Validation:** inspect `PsgFile.test.cpp`, `PsgList.test.cpp`, and `ProjectFps.test.cpp`; add cases for non-default rates, imports away from time zero, multiple clips, and legacy files. Retain damaged-tail handling and the existing parser resource-limit task. Do not duplicate those existing TODOs as newly discovered bugs.

## 9. Extract tuning export formatting

**Evidence:** [TuningViewModel.h](../src/viewmodels/tuning/TuningViewModel.h) implements `exportToCSV` and `getDefaultExportFilename`, including column order, ASCII note spelling, and filename sanitization. [TuningPreview.cpp](../src/gui/tuning/TuningPreview.cpp) owns the chooser, write, directory preference, and result alerts.

**Problem:** export-format policy lives in the stateful viewmodel alongside tuning selection and notifications. This is a lower-priority separation opportunity, not an observed correctness failure.

**Proposed boundary:** move serialization and filename formatting into a focused format helper. Keep selection of the current tuning data in the viewmodel and the save interaction in the GUI. Preserve output exactly and avoid adding an exporter interface for one format.

**Validation:** preserve the existing CSV and filename assertions in `TuningViewModel.test.cpp`, adding helper-level cases only where useful. Verify the GUI still exports the selected tuning and reports write failure. Tooltip formatting in a presentation viewmodel is not, by itself, a reason for another split.

## 10. Give PSG mutations one derived-state invalidation owner

**Evidence:** [PsgList.cpp](../src/models/PsgList.cpp) increments `dataVersion_` only in `recomputeAccumulatedState`. Each `addFrameEvent` overload invokes that method, while `removeFrameEvent` and `removeAllFrames` only change the ValueTree. `EventList` in [PsgList.h](../src/models/PsgList.h) updates frame objects and sorting after tree changes but does not notify the owning list to rebuild accumulated values or advance its version. `PsgClip` caches pitch and tuning results using that version.

**Problem:** callers must know which mutation paths repair derived state. Removing a frame after priming a clip cache leaves the cache version unchanged; removing a frame that supplied a register value can also leave later accumulated snapshots carrying that value. Direct property changes and undo/redo need the same audit. Frame property refresh currently sets present fields without clearing masks for removed properties. These are source-backed invalidation gaps; no runtime regression has been executed yet.

**Proposed boundary:** make `PsgList` own invalidation for supported mutations, including ValueTree notifications and undo/redo. Establish an explicit distinction between stored per-frame changes and accumulated playback/display state. Preserve efficient bulk import with batching or lazy rebuilding rather than recomputing the entire list for every imported property.

**Validation:** extend `PsgList.test.cpp` and `PsgClipTuning.test.cpp` with primed-cache cases for deletion, clearing, frame movement, property replacement/removal, and undo/redo. Check later inherited register values as well as pitch/tuning results. Existing round-trip and whole-clip redo tests do not establish these mutation contracts.

## 11. Include timing context in tuning cache dependencies

**Evidence:** `PsgClip::buildPitchHistogram` in [PsgClip.cpp](../src/models/PsgClip.cpp) weights the final stored frame through an end index derived from clip offset, length, and effective FPS. Effective FPS depends on frames per beat and tempo at the clip start. `getTuningDetection` caches solely by PSG data version; `getTuning` additionally checks user choices, but not this timing context.

**Problem:** a length, offset, or timing change can alter histogram weights without changing the cache key. Repainting the component does not invalidate these model caches. This is a mismatch between calculation inputs and cache dependencies, separate from missing data-version updates in finding 10. A change need not alter the winning key to require recalculation of rankings and confidence.

**Proposed boundary:** define the effective histogram inputs once and use them consistently for detection and resolved-tuning cache validity. A key containing the derived end-frame index and data version may be sufficient for the current histogram; verify the complete dependency chain before choosing it. Preserve explicit table/key choices while updating automatic rankings and the tuning revision consumed by the UI.

**Validation:** prime both caches, then change clip length/offset, frames per beat, and tempo context; compare against freshly computed detection. Cover moving across a tempo boundary and undo/redo. Extend `PsgClipTuning.test.cpp`, whose current cache test checks reuse and explicit choice changes. Decide source-versus-visible-clip histogram semantics separately from the cache fix.

## 12. Make widget bindings own callback teardown

**Evidence:** [ParamBindings.cpp](../src/gui/common/ParamBindings.cpp) installs widget callbacks capturing the binding's `this`: slider value, drag, and text-conversion callbacks; combo change; and button/toggle click handlers. Its base destructor removes the endpoint listener, but the derived bindings in [ParamBindings.h](../src/gui/common/ParamBindings.h) do not remove those widget callbacks. The mouse-listener member does detach its own registration.

**Problem:** a widget that survives destruction of its binding retains callbacks into the destroyed binding. Existing compound controls generally destroy binding and widget together, so this review does not establish a crash in their current normal teardown. The reusable binding classes nevertheless expose an incomplete lifetime contract for independently scoped bindings or widget reuse.

**Proposed boundary:** give each binding explicit ownership of the callbacks it installs and detach them during destruction, including slider formatting/parsing callbacks. Check the policy for pre-existing or subsequently replaced callbacks before implementing cleanup; do not erase callbacks owned by a newer binding. Document that the widget must outlive its attachment and avoid a general callback registry.

**Validation:** extend `ParamBindings.test.cpp` to destroy a binding while retaining its widget, then exercise value changes, drag gestures, text conversion, and clicks. Check rebinding and owner destruction order. Do not confuse endpoint-listener removal with widget callback cleanup.

## 13. Centralize persisted PSG parameter mapping

**Evidence:** [PsgList.cpp](../src/models/PsgList.cpp) repeats parameter/property knowledge in `convertPsgFrameFromStrings`, `PsgParamFrame::createPsgFrameValueTree`, and `PsgParamFrame::updatePropertiesFromState`: an explicit conversion list, a serialization switch, and a deserialization if/else chain. `RetriggerEnvelope` maps to `IDs::re` in the reader/writer but is absent from the string-conversion list.

**Problem:** adding or changing one persisted parameter requires synchronized edits in three representations. The envelope-retrigger omission demonstrates drift, though downstream numeric conversion may mask its runtime effect; this is not a claim of proven data loss.

**Proposed boundary:** keep one explicit parameter-to-property mapping at the persistence boundary and reuse it for writing, reading, and legacy string normalization. Keep frame-index/version metadata separate. Preserve existing property names, sparse masks, and unknown-property handling; use a small table or equivalent existing mechanism, not a reflection framework.

**Validation:** add a round-trip case covering every persisted parameter, including envelope retrigger, absent versus zero-valued fields, string-valued legacy state, and unknown properties. Use the current `PsgList.test.cpp` round-trip coverage as a starting point. Coordinate property-removal semantics with finding 10.

## 14. Give deferred preview requests cancellation semantics

**Evidence:** `MultitrackMidiPreview::startPlayback` in [MultitrackMidiPreview.cpp](../src/viewmodels/tuning/MultitrackMidiPreview.cpp) posts a message-thread callback holding a weak object reference and a captured playback duration. `stopPlayback` stops the transport but does not invalidate pending start callbacks. `TuningPlayer::stopNotes` and its tuning-change handler in [TuningPlayer.cpp](../src/viewmodels/tuning/TuningPlayer.cpp) use that stop operation.

**Problem:** object lifetime and request lifetime are different. If start is queued and stop is called before the queue drains, the callback still requests playback. Several queued starts can also apply old durations to the latest mutable clips. The weak reference prevents use after destruction, but does not reject work that has been superseded. This ordering follows from the source; an audible/UI reproduction has not been run.

**Proposed boundary:** make the preview object own the current playback request as well as transport access. A request revision checked by the deferred callback, invalidated by stop and replacement, is sufficient if all these operations remain on the message thread. Keep the existing weak lifetime check and re-check request validity around any operation that may dispatch pending work. Do not introduce a task framework for this one queue.

**Validation:** extend `MultitrackMidiPreview.test.cpp` with start-then-stop before queue drain, two starts with different durations, tuning-change cancellation, and destruction with a pending request. Existing tests flush the message queue after preparing clips but do not establish cancellation or supersession behavior. Verify note highlights agree with the final transport state.

## 15. Give track rendering a single commit and rollback boundary

**Evidence:** `renderSelectedTracksToAudioTrack` in [Helpers.cpp](../src/util/Helpers.cpp) chooses a file, temporarily unmutes the source, builds render parameters, runs rendering, inserts a destination track/clip, mutes the source, and updates selection. It restores the source mute state when the output file is absent, but if `insertWaveClip` returns null it still mutes the source and selects the newly added empty track.

**Problem:** success is spread across several side effects with incomplete rollback. A failed clip insertion is treated as a completed render-to-track operation. Output-file existence is also used as the completion check, so cancellation and partial-output behavior need verification against the engine API before deciding how to classify them.

**Proposed boundary:** separate preparing/running the render from committing its result to the edit. Make temporary source-state restoration scoped, and commit source muting and selection only after the destination clip exists. On failure, restore the original mute state and remove only the newly created empty destination. Define ownership of partial output and cleanup explicitly; retain warnings and undo behavior. Keep this as a focused operation rather than a generic transaction framework.

**Validation:** cover render failure, cancellation, clip insertion failure, and success with initially muted and unmuted sources. Check track count, selection, mute state, and output-file handling after each outcome. No dedicated render-operation tests were found. Inspect Tracktion render completion semantics and perform native render/cancel checks. The existing TODO about quitting during a render remains a related, separate lifetime check.

## 16. Move scope menu policy out of generic device UI

**Evidence:** [PluginDeviceUI.h](../src/gui/devices/PluginDeviceUI.h) declares `addScopeSettingsMenu`, and [PluginDeviceUI.cpp](../src/gui/devices/PluginDeviceUI.cpp) includes scope-specific settings/trigger types and defines their presets. Its callers are [AYPluginEditor.cpp](../src/plugins/uZX/aychip/AYPluginEditor.cpp) and two entry points in [ScopePluginUI.cpp](../src/plugins/uZX/scope/ScopePluginUI.cpp).

**Problem:** the shared device UI abstraction owns one visualization's settings policy. Scope preset changes therefore alter the generic module, even though the helper is already correctly shared by its consumers. This is a layering cleanup, not an observed runtime failure.

**Proposed boundary:** move the helper into a scope-specific UI module and update all three callers. Keep generic parameter menu helpers in `PluginDeviceUI` if they remain shared. Reuse one scope helper; do not duplicate it for AY and Scope or add another registry.

**Validation:** preserve labels, presets, checked states, and settings mutations at all three menu entry points. Update `src/sources.cmake` if a source file is added and build the affected GUI consumers. No behavior change or new format is needed.

## 17. Make MIDI sample-range semantics consistent

**Evidence:** `MidiBufferContext` in [MidiEffect.h](../src/plugins/uZX/midi_effects/MidiEffect.h) stores a start sample and a sample count; `sliced` retains sample coordinates relative to the original buffer. `MPEInstrumentFx::operator()` in [MPEInstrumentFx.h](../src/plugins/uZX/midi_effects/MPEInstrumentFx.h) starts at `c.start`, but rejects events at `eventSample >= c.length` and creates its final slice with `c.length - currentSample`.

**Problem:** a count is treated as an absolute end in two places. For start 64 and length 64, an event at sample 96 belongs to the range [64, 128), but is rejected; the final slice can also have the wrong length. The host wrapper currently asserts `bufferStartSample == 0`, so the review establishes a mismatch in the reusable context contract, not a confirmed failure of the current normal host path.

**Review boundary:** decide whether effects support arbitrary context starts. If they do, define the exclusive end once from start plus length and use it for event inclusion and remaining render spans. If zero-start processing is intentionally required, make that restriction explicit at the appropriate entry boundary while preserving nonzero voice sub-blocks. Do not fix only the event comparison and leave the tail calculation inconsistent.

**Validation:** add focused MPE effect cases for zero and nonzero start, no events, events at start/end, and an event inside the range; verify total rendered samples and nonnegative slice lengths. Inspect production context construction and slicing callers before changing assumptions. The existing TODO already notes missing instrument/voice tests; this supplies a concrete first regression case.

## 18. Align command registration with supported app capabilities

**Evidence:** [MainCommands.h](../src/controllers/MainCommands.h) registers Cut, Copy, and Paste, supplies their keyboard shortcuts, and adds them to the Studio Edit menu. `AppController::perform` in [MainController.cpp](../src/controllers/MainController.cpp) has no handlers for these IDs and returns false through its default case; `getNextCommandTarget` returns null. App-specific capability rules are also spread across menu construction, command status, and dispatch: Player registers Save/Save As, can report them active with an edit, but skips their actions and returns true.

**Problem:** advertised command availability and implemented behavior have separate owners. The application-level Cut/Copy/Paste routes have no implementation; Player can silently consume unsupported save commands. This does not mean focused text editors lack their own clipboard behavior. Native menu and keyboard invocation have not been tested during the review.

**Proposed boundary:** define supported command IDs per app in one place and use that knowledge for registration/menu availability and status checks. Keep dispatch explicit and preserve stable IDs. Remove or disable unsupported actions until there is concrete behavior; implementing clipboard editing is a separate feature. Avoid replacing the existing command system with a generic dispatch framework.

**Validation:** verify every exposed command has a supported handler for that app without blindly invoking destructive commands in a test loop. Add focused checks for unsupported IDs, Player capabilities, and selection-dependent enablement. Exercise Studio Edit menu and keyboard routes, including text-field focus. Update the user guide if visible commands or bindings change.

## 19. Make scope sample publication data-race-free

**Evidence:** [ScopeBuffer.h](../src/plugins/uZX/scope/ScopeBuffer.h) stores samples in an ordinary `std::array<float, 8192>`. The producer writes these slots then publishes an atomic write index; `copyTo` acquires the index and copies the slots with `memcpy` while production can continue. [ScopePlugin.cpp](../src/plugins/uZX/scope/ScopePlugin.cpp) and [AYPlugin.cpp](../src/plugins/uZX/aychip/AYPlugin.cpp) push from rendering; [ScopePluginUI.cpp](../src/plugins/uZX/scope/ScopePluginUI.cpp) copies on the UI timer. A supported 4096-sample display window requests 8192 samples for trigger search, covering the entire ring.

**Problem:** publishing the index orders earlier writes, but does not stop subsequent writes from overlapping the reader's ordinary sample accesses. The buffer has no ownership handoff or protection for overwritten slots, so overlapping reads/writes constitute a C++ data race. Lossy visualization is acceptable; unsynchronized memory access is not. The unsafe path is established from source, but sanitizer reproduction and visible waveform effects have not been measured.

**Proposed boundary:** make `ScopeBuffer` own a race-free publication contract for visualization data. Evaluate atomic sample slots if they are lock-free on supported targets and mixed-generation snapshots are acceptable; otherwise use a bounded ownership handoff for snapshots. Preserve a nonblocking audio producer and explicit dropped-frame policy. Audit the documented UI-thread `clear` operation too; its current production use is initialization, not proof of a live clear race. An atomic index alone, or a retry counter around ordinary `memcpy`, does not establish safe sample access.

**Validation:** add focused wraparound and concurrent producer/consumer coverage, including the full-capacity read used by the largest window. Use a thread sanitizer run for publication changes where supported and measure audio-thread overhead. Verify both standalone Scope and AY visualization consumers. Coordinate with finding 6, but keep this buffer contract testable independently of emulator configuration.

## 20. Let scope displays own their settings binding

**Evidence:** `AYPluginUI::updateScopeDisplayParams` in [AYPluginEditor.cpp](../src/plugins/uZX/aychip/AYPluginEditor.cpp) and `ScopePluginUI::updateDisplayParams` in [ScopePluginUI.cpp](../src/plugins/uZX/scope/ScopePluginUI.cpp) repeat the same four setting reads, trigger-strategy conversion, and per-display setter calls. Both parents also duplicate listener registration and removal for those settings. `WaveformDisplay` already receives and retains `ScopeSettings&` for its menu.

**Problem:** each parent must know how the same display consumes scope settings. Adding a setting or changing its interpretation requires editing both parent bindings even though the display already has access to the authoritative settings object. This is a confirmed duplication/ownership seam, not an observed behavior failure.

**Proposed boundary:** move settings subscription, initial application, and teardown into `WaveformDisplay`, which already holds the settings reference. Let parents choose channels, layout, labels, and colors. Remove their duplicated settings propagation, retaining the settings object's existing lifetime relative to the displays. Check notification cost with multiple displays; no general binding framework is needed.

**Validation:** verify initial persisted values and live changes to window, gain, trigger mode, and level for both AY and standalone Scope. Exercise creation/destruction of multiple displays sharing settings, confirm listeners detach, and retain existing waveform/menu behavior. This complements finding 16 without coupling the UI cleanup to the sample-publication change in 19.

## Deferred review notes

The build/test inventory contains a duplicate `TimelineGrid.test.cpp` entry, two commented-out prototype test sources (`CustomClip.test.cpp` and `PsgTrack.test.cpp`), and an empty `CustomTuning.test.cpp`. Their presence alone does not establish missing coverage of supported behavior. Check prototype intent and replacement tests before deleting or re-enabling them; no build-system redesign is justified by this inventory alone.

A parameter live-reader slot also has implicit exclusive-ownership assumptions. No overlapping-owner production path was established, so this is not promoted to a separate refactoring task. Revisit it only if lifecycle tracing demonstrates overlapping attachment or replacement.

Async UI actions need a separate lifetime check: `TitleBar::showDeviceMenu` in [DevicePanelItem.cpp](../src/gui/devices/DevicePanelItem.cpp) captures `this` for Remove Device; the transport FPS menu does likewise, and file-chooser callbacks retain engine/controller references. Native menu/chooser cancellation guarantees on component destruction were not verified, so these captures alone are not reported as proven crashes. Trace dismissal and shutdown first; where an action should survive its view, capture the appropriate model owner, and where it should be cancelled, use a lifetime guard. Do not keep a whole edit alive accidentally just to preserve a menu action.

View-state validation was inspected but not promoted to another task. Gesture zoom paths clamp their scale, while persisted/direct zoom and track-height setters have weaker invariants. Verify supported load/caller inputs before treating malformed state as a current product failure; keep any later normalization at the owning state boundary rather than adding guards to every painter.
