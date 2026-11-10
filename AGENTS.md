# Repository Guidelines

## Project Structure & Module Organization

uZX is a C++23 JUCE/Tracktion Engine application. App and library code lives under `src/`: `controllers`, `models`, `viewmodels`, `gui`, `plugins/uZX`, `formats`, and `util`. `src/sources.cmake` is authoritative for source membership: add shared code to `SHARED_SOURCES`, UI/app-only code to `GUI_SOURCES`, and tests to `TEST_SOURCES`. Unit tests are colocated with implementation as `*.test.cpp`; `tests/main.cpp` provides the JUCE test runner. Assets live in `resources/`, docs in `docs/` (index and upkeep rules in `docs/README.md`), CMake helpers in `cmake/`, and vendored dependencies in `third_party/`.

All targets — `uZX` (Studio), `uZXTuning`, `uZXPlayer`, `uZXTests` — link the `motool_common` static library and share one entry point. Per-target behavior is keyed on `ProjectInfo::projectName` in `src/controllers/App.cpp`.

## Build, Test, and Development Commands

- `git submodule update --init --depth=1`: fetch JUCE, Tracktion, zxtune, and ayumi dependencies after cloning.
- `cmake --preset default`: configure a Ninja Debug build in `build/`, arm64-only on macOS (Release stays universal). A `build/` made by another generator must be removed first.
- `cmake --preset release` then `cmake --build --preset release --parallel`: a Release build in `build-release/`, as CI ships; measure GUI performance here, not in Debug.
- `cmake --build --preset default --parallel`: build the default targets.
- `cmake --build build --target uZX --parallel`: build Studio; use `uZXTuning`, `uZXPlayer`, or `uZXTests` for other targets. Always pass `--parallel` to CMake build commands.
- `ctest --preset default --output-on-failure`: run CTest after `uZXTests` has been built.
- `build/src/uZXTests_artefacts/Debug/uZXTests AYChip`: run tests whose name contains the filter; `--list-tests` lists them.
- `./format-code.sh -n` then `./format-code.sh`: dry-run and apply clang-format.

## Coding Style & Naming Conventions

Use `.clang-format`: 4 spaces, no tabs, 120 columns, C++23, sorted include blocks, and left-aligned pointers. Namespaces use `MoTool::` with sub-namespaces where appropriate. Classes and types use PascalCase; functions and methods use camelCase; private members use a trailing underscore; constants use upper-case or enum-style PascalCase. Put opening braces on the same line for functions and on a new line for classes and structs.

Use `/** */` for public interface docs and `//` sparingly for non-obvious logic. Section dividers are a `//====...` line followed by a `/** */` block. Use JUCE's `Optional` for nullable returns.

Do not add `#include <tracktion_engine.h>` or a `namespace te` alias: Tracktion is available through `<JuceHeader.h>`. Write `tracktion::Edit`, `tracktion::MidiClip`, etc. For engine behavior, read the source in `third_party/tracktion_engine/` rather than guessing.

## Cross-Compiler Pitfalls

CI builds with Clang (macOS), GCC (Linux), and MSVC (Windows). Clang is the most permissive, so code that builds locally on macOS can still fail CI.

- GCC `-Wchanges-meaning`: `using Foo = Foo;` inside a class when `Foo` also exists in the enclosing namespace. Qualify it: `using Foo = MoTool::Foo;`.
- GCC `-Wreturn-type`: a `switch` over an enum without `default` is not seen as exhaustive. End the function with `jassertfalse; return {};`.
- GCC `-Wshadow`: a constructor parameter named like an inherited member. Rename the parameter (e.g. trailing `_`).
- GCC `-Wunused-but-set-variable`: a variable used only inside `DBG()`, which is empty in Release. Use `juce::ignoreUnused(x)`.
- GCC `-Wfloat-equal`: `==`/`!=` on floats. Wrap intentional comparisons in `JUCE_BEGIN/END_IGNORE_WARNINGS_GCC_LIKE("-Wfloat-equal")`.
- `JUCE_MAC`, `JUCE_WINDOWS` and `JUCE_LINUX` are defined only on their own platform, so `JUCE_MAC ? a : b` is an undeclared identifier elsewhere. Test them with `#if`.
- MSVC rejects `[[maybe_unused]]` on a range-for variable. Use `juce::ignoreUnused(var)` in the loop body.
- MSVC: `String::formatted("%s", str.toUTF8())` garbles text because `%s` expects `wchar_t*` on Windows. Concatenate instead: `"prefix " + str + String::formatted(", val=%.2f", x)`.

## Testing Guidelines

Add JUCE `UnitTest` classes in adjacent `*.test.cpp` files and register them with a static instance. Name tests by feature, such as `TuningViewModel` or `AYChip`, so direct binary filtering remains useful. Cover changed model, tuning, plugin, and controller behavior; add regression tests for bug fixes. Remember to list new test files in `TEST_SOURCES`.

## User Documentation

A change to anything a user can see or do updates `docs/user-guide.md` in the same commit. That covers key bindings (`addDefaultKeypress` in `src/controllers/MainCommands.h`), mouse and wheel handlers, menus, and controls. **Keyboard and Mouse** in the guide is the single list of shortcuts and gestures; it must match what the code actually does in each app. Don't document a binding that does nothing.

## Commit & Pull Request Guidelines

Prefer concise imperative messages; use a lowercase scope when helpful (`docs:`, `ci:`, `build:`). Never commit to `develop` directly: branch from it as `feature/<name>` or `fix/<name>`, commit there, and merge back into `develop` (a PR, or a local `--no-ff` merge when asked), per **Branching Model** in `CONTRIBUTING.md`; the exceptions are release version bumps and commits that touch only `docs/`. `main` is stable and protected, so never push to it directly. Releases are cut by pushing `v*` tags; see `CONTRIBUTING.md`. PRs should describe behavior changes, link issues, list tests run, and include screenshots or recordings for UI changes.

Before pushing a `feature/` or `fix/` branch and opening its PR, add what it changes for users to `CHANGELOG.md` under **[Unreleased]**, in the section of the area it touches (a new section if none fits): a short line per feature, and a `Fixed:` line for fixes. Commit the entry on the branch, so it merges with the change.

## Agent-Specific Instructions

Do not edit generated build trees or vendored `third_party/` code unless explicitly required. Keep build artifacts out of commits.

For native macOS UX testing, screenshots, and keyboard/mouse control, use
`./scripts/peekaboo.sh`; read `docs/ux-testing.md` first. Run desktop operations
outside the sandbox, target the exact uZX PID/window, and observe after each
action. Save screenshots and JSON under `.cache/ux/`.
Regenerate the user-guide figures with `python3 -B scripts/illustrate_user_guide.py`;
the generator owns and closes its test app sessions, and refuses existing ones.
