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
- `.cpp` files go in `SHARED_SOURCES` (or `GUI_SOURCES` for UI/app-only code)
- `.test.cpp` files go in `TEST_SOURCES`

## Architecture Overview

MoTool is built on [Tracktion Engine](https://github.com/Tracktion/tracktion_engine/) and [JUCE](https://juce.com/). Key architectural docs are in the `docs/` directory:

- `docs/Design.md` — overall architecture
- `docs/OVM Design pattern.md` — state management pattern
- `docs/Parameter binding.md` — parameter binding system
- `docs/Tuning Systems.md` — tuning system design

## Submitting Changes

### Branching Model

- **`develop`** — active development branch. All feature branches merge here.
- **`main`** — stable branch. Protected: requires PRs and passing CI status checks. No direct pushes.
- **Feature branches** — branch off `develop`, merge back into `develop` via PR.

### Workflow

1. Fork the repository (or work in a branch if you have write access).
2. Create a feature branch from `develop`:
   ```bash
   git checkout develop && git pull
   git checkout -b feature/my-feature
   ```
3. Make your changes with tests where applicable, and make sure `uZXTests` passes.
4. Push and open a pull request targeting `develop` with a clear description of your changes:
   ```bash
   git push -u origin feature/my-feature
   ```
5. After review and CI pass, the PR is merged.

## Release Process

Releases are triggered by pushing a `v*` tag. Tags can be created on either `develop` or `main`:
- **`develop`** — alpha/dev pre-releases (tagged with `-alpha`, `-dev`, etc.)
- **`main`** — stable releases only (tagged without suffix)

The `main` branch is protected, so changes must go through a PR.

### Alpha/Pre-release (from `develop`)

1. **Bump the version** on `develop`:
   ```bash
   git checkout develop
   # Edit versions.cmake — update PROJECT_CORE_VERSION
   git add versions.cmake
   git commit -m "bump version to 0.4.9"
   git push origin develop
   ```

2. **Tag `develop` HEAD** and push:
   ```bash
   git tag v0.4.9-alpha
   git push origin v0.4.9-alpha
   ```

No PR to `main` needed — this triggers the full release pipeline directly from `develop`.

### Stable Release (from `main`)

1. **Bump the version** on `develop`:
   ```bash
   git checkout develop
   # Edit versions.cmake — update PROJECT_CORE_VERSION
   git add versions.cmake
   git commit -m "bump version to 0.5.0"
   git push origin develop
   ```

2. **Create a PR** from `develop` to `main`:
   ```bash
   gh pr create --base main --head develop \
     --title "Release v0.5.0" \
     --body "Stable release v0.5.0."
   ```

3. **Wait for CI**, then merge the PR.

4. **Tag the merged commit** on `main`:
   ```bash
   git fetch origin main
   git tag v0.5.0 origin/main
   git push origin v0.5.0
   ```

Both flows trigger the full release pipeline: build + test + package on macOS/Linux/Windows, then upload artifacts to a GitHub Release with SHA256 checksums.

### Version Tag Convention

| Tag | Branch | Meaning |
|-----|--------|---------|
| `v0.5.0` | `main` | Stable release |
| `v0.4.9-alpha` | `develop` | Alpha pre-release |
| `v0.4.9-dev` | `develop` | Dev pre-release |

The version in `versions.cmake` (`PROJECT_CORE_VERSION`) should match the numeric part of the tag.

### What CI Does

| Trigger | Jobs |
|---|---|
| Push to `main` or `develop` | Build + test on all platforms |
| PR to `main` or `develop` | Build + test on all platforms |
| Tag push (`v*`) | Create GitHub Release, build + test + package + upload on all platforms, generate SHA256SUMS.txt |

## Reporting Issues

Please use the [GitHub issue tracker](https://github.com/Pixel-Matter/uZX/issues) with the provided templates for bug reports and feature requests.

## License

By contributing, you agree that your contributions will be licensed under the [GPL-3.0 License](LICENCE.md).
