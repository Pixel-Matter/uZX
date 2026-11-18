# Player v1.0 UX review

Open findings of the v1.0 UX readiness review (2026-11-18, `develop` at `984256b`), behind the **UX quality review** item in [ROADMAP.md](ROADMAP.md). A fixed finding is deleted here; what it changed for users goes to the [CHANGELOG](../CHANGELOG.md). Severity is judged against the v1.0 goal: a macOS app anyone can download and open with one documented step.

Not covered yet: a live run on macOS (Gatekeeper, the DMG, Finder associations, the menu bar) and audio playback.

## Medium

| # | Finding | Where | Suggested fix |
| --- | --- | --- | --- |
| M1 | **`.ay` is offered but not supported.** It is among the PSG extensions, so the Open dialog lists it and drops accept it, but a real `.ay` file (the AYEmul Z80 format) only gets "Can't Open File". | `Helpers::getTuneFileWildcard` in `src/util/Helpers.cpp` | Remove `ay` until it is supported, or name the format in the error. |
| M2 | **Menu wording is inconsistent.** "Play/Stop" while the button and guide say Play/Pause; "Zoom to project", "Zoom in", "Render to audio" in sentence case beside "Previous Zoom", "Follow by Page"; "Audio/MIDI" and "Plugins" open windows without "..."; the window they open is titled "Audio Settings"; "Hex params in hover" in the clip menu. | `MainAppCommands::getCommandInfo` in `src/controllers/MainCommands.h`; `src/gui/timeline/PsgClipComponent.cpp` | Title case throughout, "..." on items that open a window, "Play/Pause", "Show Hex Values on Hover". |
| M3 | **"Edit" is jargon in the Player.** The guide talks of "the open edit", while the error dialogs say "project". | `docs/user-guide.md` (Saving) | Use "project" everywhere a user reads it. |
| M4 | **Keyboard-only commands are hard to find.** The Player's Transport menu has no End, Step or Frame items, so `End`, `Left`/`Right` and `Option+Left`/`Right` show up nowhere in the app. | `AppController::getMenuForIndex` in `src/controllers/MainController.cpp` | Add them to the Transport menu, where the shortcut is shown next to each. |
| M5 | **Transport readouts give no hint they can be edited.** bpm, fpb, groove, sig and pos are click-to-type and fps is click-to-pick, with no tooltip. | `src/gui/common/Transport.cpp` | A one-line tooltip on each readout, such as "Click to type a tempo". |
| M6 | **Windows and Linux: a second launch starts a whole app before it hands over.** It loads and saves the shared startup edit first (see [TODO.md](TODO.md), Global). On Linux it cannot hand the file over at all, because JUCE's `MessageManager::broadcastMessage` is a TODO there: opening a file from the file manager while the Player runs does nothing. | `MoToolApp` in `src/controllers/App.cpp`; JUCE `juce_Messaging_linux.cpp` | Check for the other instance before constructing the controllers. On Linux, hand files over through a socket or a lock file, or allow several instances. |
| M7 | **Accessibility basics.** Tab does not reach the transport or chip controls. Screen readers get button names but no titles or descriptions for the readouts, knobs or scopes. The dim text colour (slate-500 on slate-900) is about 3.7:1, under the 4.5:1 WCAG asks for small text. | `src/gui/common/LookAndFeel.h`; no `setTitle`/`setDescription` in `src/gui` | Give readouts and knobs a title and the transport a focus order. Raise disabled and hint text to slate-400. |

## Low

- **Two different µ characters.** The app names use the Greek μ (U+03BC: `src/CMakeLists.txt`, `App.cpp`); the README, the plist's "µZX Project" and the error dialogs use the micro sign µ (U+00B5), which `Option+M` types on a Mac. Spotlight searched with `Option+M` won't find "μZX Player". Pick one.
- **Linux packaging.** The tarball holds a bare binary named `μZX Player`, with a space and a non-ASCII letter, and no `.desktop` file or icon (`.github/workflows/ci.yml`). Its Open dialog, JUCE's built-in one when zenity and kdialog are missing, starts with "μZX Player" typed into the file name field.
- **Windows packaging.** An unsigned `.exe` in a zip triggers SmartScreen, and nothing says so. The ROADMAP defers the installer and signing until after 1.0.
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
