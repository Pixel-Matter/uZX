# Refactoring plan

Findings from a source-inspection review (November 2026) of controller lifecycle and commands, PSG import and serialization, timeline components, derived-state caches, parameter bindings, tuning export, and audio configuration paths. Findings separate confirmed code structure from runtime risks that still need reproduction; nothing here was established by running the app. It is not an exhaustive audit.

All of its findings are done and deleted; new ones go here with the next free number, and on the [TODO.md](TODO.md#refactoring) checklist. Numbers are stable IDs, not an order. The deferred notes below were inspected and not promoted to findings.

Keep each step small and behavior-preserving: establish the ownership boundary first, then change behavior separately. Do not introduce a general framework for these extractions. If a step changes anything users can see or do, update `user-guide.md` in the same commit.

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

Audio-thread locking was reviewed and measured (finding 6, closed) without moving configuration to block boundaries. `AYChipPlugin` and `NotesToPsgMapper` each hold one lock across rendering. On the message thread, the clock, chip type, MIDI channel and tuning setters, `reset`, and register reads take it for O(1) work. `initialise` creates the chip and sizes the scope lanes under it, because the old graph may still be rendering. `deinitialise` runs only once no graph node holds the plugin. The replaced tuning is now freed after the mapper's lock is released. In a Debug build, about 3,300 chip setting changes a second left AY block times unchanged (p99 about 340 µs, the same as idle). About 14,000 retunes a second raised the mapper's p99.9 from 4–11 µs idle to 15 µs, or 20–25 µs before the tuning was freed outside the lock. The worst blocks were OS scheduling outliers, which also occur idle. Revisit only if a glitch is heard while settings change.

View-state validation was inspected but not promoted to another task. Gesture zoom paths clamp their scale, while persisted/direct zoom and track-height setters have weaker invariants. Verify supported load/caller inputs before treating malformed state as a current product failure; keep any later normalization at the owning state boundary rather than adding guards to every painter.
