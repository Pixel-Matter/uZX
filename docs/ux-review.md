# Player v1.0 UX review

Open findings of the v1.0 UX readiness review (2026-11-18, `develop` at `984256b`), behind the **UX quality review** item in [ROADMAP.md](ROADMAP.md). A fixed finding is deleted here; what it changed for users goes to the [CHANGELOG](../CHANGELOG.md). Severity is judged against the v1.0 goal: a macOS app anyone can download and open with one documented step.

Not covered yet: a release build run on macOS (Gatekeeper, the DMG, Finder associations) and listening to playback. The menus, the accessibility tree and a second launch were checked in a Debug Player.

## Medium

| # | Finding | Where | Suggested fix |
| --- | --- | --- | --- |
| M7 | **Tab does not reach the transport or chip controls.** Screen readers now name the readouts, knobs, scopes and meters, and no text to read is below 4.5:1. Left: no control takes Tab focus, and the look-and-feel draws no focus ring. A focused knob or button would also take the arrows and Space the Player steps and plays with. | `src/gui/common/Transport.cpp`, `src/plugins/uZX/aychip/AYPluginEditor.cpp`, `src/gui/common/LookAndFeel.cpp` | Focus by Tab only (`setMouseClickGrabsKeyboardFocus(false)`), a focus ring in the look-and-feel, and Escape to give the keys back to the transport. |
