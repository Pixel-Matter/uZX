# Player v1.0 UX review

Open findings of the v1.0 UX readiness review (2026-11-18, `develop` at `984256b`), behind the **UX quality review** item in [ROADMAP.md](ROADMAP.md). A fixed finding is deleted here; what it changed for users goes to the [CHANGELOG](../CHANGELOG.md). Severity is judged against the v1.0 goal: a macOS app anyone can download and open with one documented step.

Not covered yet: a live run on macOS (Gatekeeper, the DMG, Finder associations, the menu bar) and audio playback.

## Medium

| # | Finding | Where | Suggested fix |
| --- | --- | --- | --- |
| M7 | **Tab does not reach the transport or chip controls.** Screen readers now name the readouts, knobs, scopes and meters, and no text to read is below 4.5:1. Left: no control takes Tab focus, and the look-and-feel draws no focus ring. A focused knob or button would also take the arrows and Space the Player steps and plays with. | `src/gui/common/Transport.cpp`, `src/plugins/uZX/aychip/AYPluginEditor.cpp`, `src/gui/common/LookAndFeel.cpp` | Focus by Tab only (`setMouseClickGrabsKeyboardFocus(false)`), a focus ring in the look-and-feel, and Escape to give the keys back to the transport. |

## Low

- **Two different µ characters.** The app names use the Greek μ (U+03BC: `src/CMakeLists.txt`, `App.cpp`); the README, the plist's "µZX Project" and the error dialogs use the micro sign µ (U+00B5), which `Option+M` types on a Mac. Spotlight searched with `Option+M` won't find "μZX Player". Pick one.
- **Viz mode with no tune** shows a blank black screen, since tracks without clips are hidden. Disable **Viz Mode** until a tune is open (`AppController::getCommandInfo` checks only for an edit).
- **The ruler menu adds Studio's "Preserve PSG Timing"** in the Player too; JUCE skips the unregistered command, so nothing shows. Harmless, but it reads as a bug (`src/gui/timeline/Ruler.cpp`).
- **The empty Player opens at 120 bpm with a "5 5 5 5 5" groove** at 25 fpb, values no tune has given it. They could stay blank until a file is open.

## Docs and README

The user guide matches the code; the problem is readability. A newcomer meets 600-word paragraphs before learning how to play a file.

| # | Finding | Where | Suggested fix |
| --- | --- | --- | --- |
| D1 | **No quick start.** The Player section opens with file-type details; the TAYM paragraph alone is about 600 words in one block. | `docs/user-guide.md` (Opening Files) | Open the Player section with three steps: drop or open a file, `Space`, `F` for Viz mode. Move format details to a "File formats" section and split long paragraphs. |
| D3 | **The fpb readout is missing** from the Transport Bar table, though the Player shows it between fps and groove. | `docs/user-guide.md` (Transport Bar) | Add a row. |
| D4 | **No troubleshooting.** Nothing on no sound or picking an audio device, and nothing for Windows SmartScreen or running the Linux binary. | `docs/user-guide.md` | A short "If something goes wrong" section; install notes per platform, or a line saying v1.0 supports macOS only. |
| D5 | **The ffmpeg install is macOS-only:** `brew install ffmpeg`. | `docs/user-guide.md` (Render Video) | Add `winget install ffmpeg` and `sudo apt install ffmpeg`. |
| D6 | **The Discord link is a channel URL**, which opens only for people already in the server. | `README.md` | Use an invite link. |
| D7 | **README promises more platforms than v1.0 ships.** The badge lists iOS and Android; the intro says macOS, Windows and Linux, while the ROADMAP calls v1.0 a macOS release. | `README.md` | Drop iOS and Android from the badge; say which platforms are supported and which are previews. |
| D8 | **Small mismatches.** README says **FPS**, the app shows **fps**. README builds with `cmake -S . -B build`, bypassing the presets CONTRIBUTING describes. | `README.md` | Align the wording; use `cmake --preset release` in the README. |
