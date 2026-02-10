# μZX Player

Lightweight playback-only variant of μZX Studio. Same codebase, reduced feature set. Users can open `.uzx` projects and `.psg` files for playback without editing capabilities.

## Architecture

Follows the multi-target pattern established by `uZXTuning`. A separate CMake target (`uZXPlayer`) shares the `motool_common` static library and `Main.cpp` entry point with the other targets. Target detection uses `ProjectInfo::projectName` in `App.h`.

### Key Classes

| Class | File | Role |
|-------|------|------|
| `PlayerController` | `controllers/PlayerController.h/.cpp` | Edit lifecycle, PSG replace-all import, zoom |
| `PlayerDocumentComponent` | `gui/main/PlayerDocument.h/.cpp` | Top-level layout: transport + timeline + AY panel |
| `AYPluginSidePanel` | `gui/devices/AYPluginSidePanel.h/.cpp` | Shows AYChipPlugin UI for selected track |

### Parameterized Existing Components

`TransportBar` and `EditComponent` accept options structs for backward-compatible feature toggling:

```cpp
// TransportBar — hide record button and automation controls
TransportBar(evs, {.showRecord = false, .showAutomation = false});

// EditComponent — hide bottom details panel
EditComponent(edit, evs, {.showDetailsPanel = false});
```

### Layout

```
+---------------------------------------------------------+
|  TransportBar (40px)                                    |
|  [Rewind] [Play] | BPM | frames | 4/4 | 00:00 | Vol   |
+-----------------------------------------+---------------+
|  EditComponent (no details panel)       | AYPlugin      |
|  +- Ruler --------------------------+  | Side Panel    |
|  +- Track body (no headers) --------+  | (240px)       |
|  |  [PSG Clip visualization]        |  |               |
|  |  [PSG Clip visualization]        |  | [AYPluginUI]  |
|  +----------------------------------+  |               |
+-----------------------------------------+---------------+
```

### Removed from Studio

- Record button, automation controls
- Track headers (I/R/M/S)
- Bottom details panel
- Clip/track creation, plugin add/remove, undo/redo
- Edit menu, Add menu, Track menu
- File > New, Save, Save As, Reveal, Import Audio

### Added

- Right-side AY plugin panel (240px) showing the selected track's AYChipPlugin UI directly (no FramedDeviceItem wrapper)
- File > Import PSG replaces all tracks/clips instead of appending

## Menu Structure

| Menu | Items |
|------|-------|
| **File** | Open (.uzx), Open Recent, Import PSG (replaces all), Quit |
| **Transport** | Play/Stop, Rewind |
| **View** | Zoom to Project, Zoom In, Zoom Out |
| **Settings** | Audio/MIDI Settings |
| **Help** | About |

## PSG Replace-All Import

`PlayerController::handleImportPsgReplace()`:

1. Browse for `.psg` file
2. Stop transport
3. Remove all clips from all tracks
4. Delete all audio tracks except first
5. Insert PSG clip on the remaining track
6. Ensure AYChipPlugin is on the track
7. Rename track to filename
8. Zoom to fit

## Build & Run

```bash
# Build
cmake -S . -B build && cmake --build build --target uZXPlayer

# Run
build/src/uZXPlayer_artefacts/Debug/μZX\ Player.app/Contents/MacOS/μZX\ Player
```

## File Map

```
src/
  controllers/
    App.h                  # Target enum (uZXPlayer), target detection, getPlayerController()
    App.cpp                # Conditional PlayerController creation
    MainController.h       # BaseController base class
    MainController.cpp     # Reduced menus, polymorphic command routing
    PlayerController.h     # PlayerController declaration
    PlayerController.cpp   # Player-specific edit setup, PSG replace-all
  gui/
    common/
      Transport.h/.cpp     # TransportBarOptions (showRecord, showAutomation)
    devices/
      AYPluginSidePanel.h/.cpp  # Selection-aware AY plugin panel
    main/
      PlayerDocument.h/.cpp     # PlayerDocumentComponent layout
    timeline/
      EditComponent.h/.cpp      # EditComponentOptions (showDetailsPanel)
  util/
    FileOps.cpp            # uZXPlayer returns .uzx suffix
  CMakeLists.txt           # uZXPlayer target definition
  sources.cmake            # New source files in GUI_SOURCES
```
