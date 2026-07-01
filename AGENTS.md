# Repository Guidelines

## Project Structure & Module Organization

uZX is a C++23 JUCE/Tracktion Engine application. App and library code lives under `src/`: `controllers`, `models`, `viewmodels`, `gui`, `plugins/uZX`, `formats`, and `util`. `src/sources.cmake` is authoritative for source membership: add shared code to `SHARED_SOURCES`, UI/app-only code to `GUI_SOURCES`, and tests to `TEST_SOURCES`. Unit tests are colocated with implementation as `*.test.cpp`; `tests/main.cpp` provides the JUCE test runner. Assets live in `resources/`, docs in `docs/`, CMake helpers in `cmake/`, and vendored dependencies in `third_party/`.

## Build, Test, and Development Commands

- `git submodule update --init --depth=1`: fetch JUCE, Tracktion, and ayumi dependencies after cloning.
- `cmake --preset default`: configure a Debug build in `build/`.
- `cmake --build --preset default --parallel`: build the default targets.
- `cmake --build build --target uZX --parallel`: build Studio; use `uZXTuning`, `uZXPlayer`, or `uZXTests` for other targets. Always pass `--parallel` to CMake build commands.
- `ctest --preset default --output-on-failure`: run CTest after `uZXTests` has been built.
- `build/src/uZXTests_artefacts/Debug/uZXTests AYChip`: run tests matching a filter.
- `./format-code.sh -n` then `./format-code.sh`: dry-run and apply clang-format.

## Coding Style & Naming Conventions

Use `.clang-format`: 4 spaces, no tabs, 120 columns, C++23, sorted include blocks, and left-aligned pointers. Namespaces use `MoTool::` with sub-namespaces where appropriate. Classes and types use PascalCase; functions and methods use camelCase; private members use a trailing underscore; constants use upper-case or enum-style PascalCase. Put opening braces on the same line for functions and on a new line for classes and structs.

## Testing Guidelines

Add JUCE `UnitTest` classes in adjacent `*.test.cpp` files and register them with a static instance. Name tests by feature, such as `TuningViewModel` or `AYChip`, so direct binary filtering remains useful. Cover changed model, tuning, plugin, and controller behavior; add regression tests for bug fixes. Remember to list new test files in `TEST_SOURCES`.

## Commit & Pull Request Guidelines

Prefer concise imperative messages; use a lowercase scope when helpful (`docs:`, `ci:`, `build:`). Branch from `develop` and open PRs back to `develop`; `main` is stable and protected. PRs should describe behavior changes, link issues, list tests run, and include screenshots or recordings for UI changes.

## Agent-Specific Instructions

Do not edit generated build trees or vendored `third_party/` code unless explicitly required. Keep build artifacts out of commits.
