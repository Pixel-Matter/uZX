# Refactoring plan

Open findings from a source-inspection review (November 2026) of controller lifecycle and commands, PSG import and serialization, timeline components, derived-state caches, parameter bindings, tuning export, and audio configuration paths. Findings separate confirmed code structure from runtime risks that still need reproduction; nothing here was established by running the app. It is not an exhaustive audit.

[TODO.md](TODO.md#refactoring) is the checklist. A finished finding is deleted from both files, so numbers have gaps; they are stable IDs, not an order.

Keep each step small and behavior-preserving: establish the ownership boundary first, then change behavior separately. Do not introduce a general framework for these extractions. If a step changes anything users can see or do, update `user-guide.md` in the same commit.

## Implementation order

1. **Small extractions:** 2 (AY plugin setup), then 4 (timecode menu), then 1 (edit replacement, after establishing lifecycle coverage), then 3 (inspect the inherited parameter-editor behavior before replacing its base class).
2. **Correctness reviews**, which need behavior checked before the implementation is settled: 6 (audio ownership), 7 (plugin clock coordination), 8 (import timing).
3. **PSG derived state:** 13 (mapping table) can precede 10; review 10 and 11 together, with separate regression cases.
4. **Contracts:** 12 (widget callback lifetime), 17 (establish supported nonzero-start callers first), 18 (command capabilities).
5. **Low priority:** 9 (export formatting).

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

## 17. Make MIDI sample-range semantics consistent

**Evidence:** `MidiBufferContext` in [MidiEffect.h](../src/plugins/uZX/midi_effects/MidiEffect.h) stores a start sample and a sample count; `sliced` retains sample coordinates relative to the original buffer. `MPEInstrumentFx::operator()` in [MPEInstrumentFx.h](../src/plugins/uZX/midi_effects/MPEInstrumentFx.h) starts at `c.start`, but rejects events at `eventSample >= c.length` and creates its final slice with `c.length - currentSample`.

**Problem:** a count is treated as an absolute end in two places. For start 64 and length 64, an event at sample 96 belongs to the range [64, 128), but is rejected; the final slice can also have the wrong length. The host wrapper currently asserts `bufferStartSample == 0`, so the review establishes a mismatch in the reusable context contract, not a confirmed failure of the current normal host path.

**Review boundary:** decide whether effects support arbitrary context starts. If they do, define the exclusive end once from start plus length and use it for event inclusion and remaining render spans. If zero-start processing is intentionally required, make that restriction explicit at the appropriate entry boundary while preserving nonzero voice sub-blocks. Do not fix only the event comparison and leave the tail calculation inconsistent.

**Validation:** add focused MPE effect cases for zero and nonzero start, no events, events at start/end, and an event inside the range; verify total rendered samples and nonnegative slice lengths. Inspect production context construction and slicing callers before changing assumptions. The existing TODO already notes missing instrument/voice tests; this supplies a concrete first regression case.

## 18. Align command registration with supported app capabilities

**Evidence:** App-specific capability rules are spread across menu construction in [MainCommands.h](../src/controllers/MainCommands.h), and command status and dispatch in [MainController.cpp](../src/controllers/MainController.cpp), each testing `isPlayer` on its own. Cut, Copy, and Paste were registered with shortcuts and no handlers; they are removed until clipboard editing exists.

**Problem:** advertised command availability and implemented behavior have separate owners, so a command can be listed with nothing behind it, as the clipboard commands were.

**Proposed boundary:** define supported command IDs per app in one place and use that knowledge for registration/menu availability and status checks. Keep dispatch explicit and preserve stable IDs. Remove or disable unsupported actions until there is concrete behavior; implementing clipboard editing is a separate feature. Avoid replacing the existing command system with a generic dispatch framework.

**Validation:** verify every exposed command has a supported handler for that app without blindly invoking destructive commands in a test loop. Add focused checks for unsupported IDs, Player capabilities, and selection-dependent enablement. Exercise Studio Edit menu and keyboard routes, including text-field focus. Update the user guide if visible commands or bindings change.

## Deferred review notes

The build/test inventory contains a duplicate `TimelineGrid.test.cpp` entry, two commented-out prototype test sources (`CustomClip.test.cpp` and `PsgTrack.test.cpp`), and an empty `CustomTuning.test.cpp`. Their presence alone does not establish missing coverage of supported behavior. Check prototype intent and replacement tests before deleting or re-enabling them; no build-system redesign is justified by this inventory alone.

A parameter live-reader slot also has implicit exclusive-ownership assumptions: `BindedAutoParameter` sets the slot of its `ParameterValue` in its constructor and clears it unconditionally in its destructor. Lifecycle tracing, including MIDI mapping, found no overlapping attachment or replacement, so this is not promoted to a refactoring task:

- `PluginBase::addParam` is the only place a `BindedAutoParameter` is created, once per `ParameterValue`, in the constructors of the AY and ChipInstrument plugins. Each `ParameterValue` is a member of one plugin instance, and no code rebuilds a plugin's parameter list.
- MIDI mapping creates no parameters. `MidiParameterMapping`, the widget endpoints, and Tracktion's `ParameterControlMappings` hold reference-counted pointers to the plugin's existing parameter and never write the slot.
- The mapping's reference does not let the parameter outlive the `ParameterValue` it refers to. The plugin destructors call `notifyListenersOfDeletion`, which reaches `AutomatableEditItem::deleteAutomatableParameters`; that removes the mapping and the plugin's own reference while the plugin's members are still alive. Device UIs hold a `Plugin::Ptr`, so their parameter pointers cannot outlive the plugin either.
- Mapped controller values are delivered on the message thread (`MidiInputDevice` posts them through an async updater), so they do not race the slot.

The `AYChipMappedParameter` tests cover the mapped case. The contract depends on every plugin with bound parameters calling `notifyListenersOfDeletion` in its destructor; a plugin that omits it would leave a mapped parameter clearing the slot of a destroyed `ParameterValue`.

The same tracing found a mapping defect unrelated to the slot, recorded in the Global TODO: deleting a plugin removes its controller mappings from `ParameterControlMappings` in memory, and undoing the deletion creates a new plugin object that is never mapped again, although the edit state still lists the mapping. The next save then rewrites the state from memory and drops it.

Async UI actions need a separate lifetime check: `TitleBar::showDeviceMenu` in [DevicePanelItem.cpp](../src/gui/devices/DevicePanelItem.cpp) captures `this` for Remove Device; the transport FPS menu does likewise, and file-chooser callbacks retain engine/controller references. Native menu/chooser cancellation guarantees on component destruction were not verified, so these captures alone are not reported as proven crashes. Trace dismissal and shutdown first; where an action should survive its view, capture the appropriate model owner, and where it should be cancelled, use a lifetime guard. Do not keep a whole edit alive accidentally just to preserve a menu action.

View-state validation was inspected but not promoted to another task. Gesture zoom paths clamp their scale, while persisted/direct zoom and track-height setters have weaker invariants. Verify supported load/caller inputs before treating malformed state as a current product failure; keep any later normalization at the owning state boundary rather than adding guards to every painter.
