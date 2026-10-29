# Contributing to µZX

Thank you for your interest in contributing! µZX currently targets AY-3-8910/YM2149-based platforms (ZX Spectrum, Amstrad CPC, MSX, Atari ST, etc.), but the architecture is designed to support additional sound chips in the future. This guide will help you get started.

## Building from Source

### Prerequisites

- **CMake** 3.22+
- **C++23** compiler (Clang, GCC, or MSVC)
- **JUCE** dependencies (included via submodules)
- **macOS**: Xcode command line tools
- **Linux**: ALSA, X11, and related development packages
- **Windows**: Visual Studio 2019+

### Setup

```bash
git clone --recursive https://github.com/Pixel-Matter/uZX.git
cd uZX

# If submodules weren't cloned recursively:
git submodule update --init --depth=1

# Build
cmake -S . -B build
cmake --build build
```

### Build Targets

| Target | Command                                  | Description          |
|--------|------------------------------------------|----------------------|
| Studio | `cmake --build build --target uZX`       | Full-featured editor |
| Player | `cmake --build build --target uZXPlayer` | Lightweight playback |
| Tuning | `cmake --build build --target uZXTuning` | Tuning table editor  |
| Tests  | `cmake --build build --target uZXTests`  | Unit tests           |

### Running

```bash
# macOS
build/src/uZX_artefacts/Debug/μZX.app/Contents/MacOS/μZX                        # Studio
build/src/uZXPlayer_artefacts/Debug/μZX\ Player.app/Contents/MacOS/μZX\ Player  # Player

# Tests
build/src/uZXTests_artefacts/Debug/uZXTests                   # All tests
build/src/uZXTests_artefacts/Debug/uZXTests AYChip            # Tests whose name contains "AYChip"
build/src/uZXTests_artefacts/Debug/uZXTests --list-tests      # List test names
```

### Code Signing (macOS)

Set `CODESIGN_IDENTITY` before configuring and the apps are signed after each build:

```bash
export CODESIGN_IDENTITY="Apple Development: your.email@domain.com (TEAMID)"
cmake -S . -B build
cmake --build build
```

List available identities with `security find-identity -v -p codesigning`. To sign by hand:

```bash
codesign --force --sign "YOUR_IDENTITY" --deep "build/src/uZX_artefacts/Debug/μZX.app"
codesign --verify --verbose "build/src/uZX_artefacts/Debug/μZX.app"
```

### Editor Setup (clangd)

`.clangd` reads `build/compile_commands.json`. JUCE headers are generated per app target, so build `uZX`, `uZXTuning`, and `uZXPlayer` once before expecting clean diagnostics. If clangd reports bogus errors after a CMake change, re-run `cmake -S . -B build` and restart the language server (VS Code: "clangd: Restart language server").

## Code Style

- **Namespaces**: `MoTool::` with sub-namespaces (e.g., `MoTool::uZX`)
- **Classes**: PascalCase (`AYInterface`, `PsgFile`)
- **Methods**: camelCase (`setEnvelopePeriod`, `getTonePeriod`)
- **Private members**: camelCase with trailing underscore (`psgData_`, `Ayumi_`)
- **Constants**: UPPER_CASE or PascalCase for enum values
- **Indentation**: 4 spaces, no tabs
- **Braces**: same line for functions/methods, new line for class/struct definitions

Run `./format-code.sh` before committing. Code must build warning-free on Clang, GCC, and MSVC; see the cross-compiler pitfalls in [AGENTS.md](AGENTS.md).

## Testing

Tests live alongside source files with `.test.cpp` extension (e.g., `Scales.test.cpp` tests `Scales.h`).

Tests use the JUCE `UnitTest` framework:

```cpp
class MyTest : public juce::UnitTest {
public:
    MyTest() : UnitTest("TestName", "MoTool") {}
    void runTest() override {
        beginTest("Test case description");
        expectEquals(actual, expected);
    }
};
static MyTest myTest;
```

New source files must be added to `src/sources.cmake`:
- `.cpp` files go in `SHARED_SOURCES`
- `.test.cpp` files go in `TEST_SOURCES`

## Architecture Overview

MoTool is built on [Tracktion Engine](https://github.com/Tracktion/tracktion_engine/) and [JUCE](https://juce.com/). Key architectural docs are in the `docs/` directory:

- `docs/Design.md` — overall architecture
- `docs/OVM Design pattern.md` — state management pattern
- `docs/Parameter binding.md` — parameter binding system
- `docs/Tuning Systems.md` — tuning system design

## Submitting Changes

1. Fork the repository
2. Create a feature branch from `develop` and target your pull request at `develop` (see [docs/contributing.md](docs/contributing.md))
3. Make your changes with tests where applicable
4. Ensure `uZXTests` passes
5. Submit a pull request with a clear description of your changes

## Reporting Issues

Please use the [GitHub issue tracker](https://github.com/Pixel-Matter/uZX/issues) with the provided templates for bug reports and feature requests.

## License

By contributing, you agree that your contributions will be licensed under the [GPL-3.0 License](LICENCE.md).
