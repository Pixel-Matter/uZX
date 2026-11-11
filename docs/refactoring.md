# Refactoring plan

Open findings from a source-inspection review (November 2026) of controller lifecycle and commands, PSG import and serialization, timeline components, derived-state caches, parameter bindings, tuning export, and audio configuration paths. Findings separate confirmed code structure from runtime risks that still need reproduction; nothing here was established by running the app. It is not an exhaustive audit.

[TODO.md](TODO.md#refactoring) is the checklist. A finished finding is deleted from both files, so numbers have gaps; they are stable IDs, not an order.

Keep each step small and behavior-preserving: establish the ownership boundary first, then change behavior separately. Do not introduce a general framework for these extractions. If a step changes anything users can see or do, update `user-guide.md` in the same commit.

## Implementation order

1. **Correctness reviews**, which need behavior checked before the implementation is settled: 6 (audio ownership), 8 (import timing).
2. **Contracts:** 17 (establish supported nonzero-start callers first).
3. **Low priority:** 9 (export formatting).

## 6. Establish audio-thread ownership of mutable runtime state

**Evidence:** [AYPlugin.cpp](../src/plugins/uZX/aychip/AYPlugin.cpp), in `applyToBuffer`, holds a blocking lock across event processing and rendering. `reset`, configuration callbacks, and register reads use the same lock. `NotesToPsgMapper` follows the same pattern: its setters, reset and render hold one lock.

**Problem:** UI/configuration work can contend with the render callback. The lock scope is confirmed, but audible glitches and their frequency have not been measured. Removing locks without a replacement ownership contract would introduce races.

**Review boundary:** trace every writer and reader of emulator/mapper state, including startup, reset, panic, configuration, diagnostics, and shutdown. Evaluate applying pending configuration at audio-block boundaries so one thread owns mutable processing state. Account for object allocation and destruction as well as field writes; do not assume an atomic pointer alone solves real-time ownership.

**Validation:** inspect `AYPlugin.test.cpp`, `AYChip.test.cpp`, and `NotesToPsgMapper.test.cpp`; cover configuration and reset while processing, note silencing, and shutdown. Measure callback contention under repeated UI changes.

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

## 17. Make MIDI sample-range semantics consistent

**Evidence:** `MidiBufferContext` in [MidiEffect.h](../src/plugins/uZX/midi_effects/MidiEffect.h) stores a start sample and a sample count; `sliced` retains sample coordinates relative to the original buffer. `MPEInstrumentFx::operator()` in [MPEInstrumentFx.h](../src/plugins/uZX/midi_effects/MPEInstrumentFx.h) starts at `c.start`, but rejects events at `eventSample >= c.length` and creates its final slice with `c.length - currentSample`.

**Problem:** a count is treated as an absolute end in two places. For start 64 and length 64, an event at sample 96 belongs to the range [64, 128), but is rejected; the final slice can also have the wrong length. The host wrapper currently asserts `bufferStartSample == 0`, so the review establishes a mismatch in the reusable context contract, not a confirmed failure of the current normal host path.

**Review boundary:** decide whether effects support arbitrary context starts. If they do, define the exclusive end once from start plus length and use it for event inclusion and remaining render spans. If zero-start processing is intentionally required, make that restriction explicit at the appropriate entry boundary while preserving nonzero voice sub-blocks. Do not fix only the event comparison and leave the tail calculation inconsistent.

**Validation:** add focused MPE effect cases for zero and nonzero start, no events, events at start/end, and an event inside the range; verify total rendered samples and nonnegative slice lengths. Inspect production context construction and slicing callers before changing assumptions. The existing TODO already notes missing instrument/voice tests; this supplies a concrete first regression case.

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
