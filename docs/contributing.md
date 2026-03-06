# Contributing to uZX

## Branching Model

- **`develop`** — active development branch. All feature branches merge here.
- **`main`** — stable branch. Protected: requires PRs and passing CI status checks. No direct pushes.
- **Feature branches** — branch off `develop`, merge back into `develop` via PR.

## Development Workflow

1. Create a feature branch from `develop`:
   ```bash
   git checkout develop && git pull
   git checkout -b feature/my-feature
   ```
2. Make changes, commit, and push:
   ```bash
   git push -u origin feature/my-feature
   ```
3. Open a PR targeting `develop`.
4. After review and CI passes, merge the PR.

## Building

See [CLAUDE.md](../.claude/CLAUDE.md) for full build commands. Quick start:

```bash
git submodule update --init --depth=1   # first time only
cmake -S . -B build
cmake --build build
```

## Testing

Tests use the JUCE UnitTest framework. Test files live alongside source files with a `.test.cpp` extension.

```bash
cmake --build build --target uZXTests
build/tests/MoToolTests_artefacts/Debug/uZXTests              # run all
build/tests/MoToolTests_artefacts/Debug/uZXTests AYChip        # filter by name
```

## Release Process

Releases are triggered by pushing a `v*` tag. The `main` branch is protected, so changes must go through a PR.

### Steps

1. **Bump the version** on `develop`:
   ```bash
   git checkout develop
   # Edit versions.cmake — update PROJECT_CORE_VERSION
   git add versions.cmake
   git commit -m "bump version to 0.4.9"
   git push origin develop
   ```

2. **Create a PR** from `develop` to `main`:
   ```bash
   gh pr create --base main --head develop \
     --title "Bump version to 0.4.9" \
     --body "Version bump for v0.4.9-alpha release."
   ```

3. **Wait for CI**, then merge the PR.

4. **Tag the merged commit** on `main`:
   ```bash
   git fetch origin main
   git tag v0.4.9-alpha origin/main
   git push origin v0.4.9-alpha
   ```

This triggers the full release pipeline: build + test + package on macOS/Linux/Windows, then upload artifacts to a GitHub Release with SHA256 checksums.

### Version Tag Convention

| Tag format | Meaning |
|---|---|
| `v0.5.0` | Stable release |
| `v0.4.9-alpha` | Pre-release / alpha |

The version in `versions.cmake` (`PROJECT_CORE_VERSION`) should match the numeric part of the tag.

### What CI Does

| Trigger | Jobs |
|---|---|
| Push to `main` or `develop` | Build + test on all platforms |
| PR to `main` or `develop` | Build + test on all platforms |
| Tag push (`v*`) | Create GitHub Release, build + test + package + upload on all platforms, generate SHA256SUMS.txt |
