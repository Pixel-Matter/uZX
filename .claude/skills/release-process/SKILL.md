---
name: release-process
description: GitHub release policy and CI/CD pipeline for uZX. Handles version bumping, tagging, and triggering releases. Use when bumping versions, creating alpha/dev pre-releases from develop, creating stable releases from main, or checking release status.
user-invocable: true
allowed-tools: Read, Edit, Bash, Glob, Grep, AskUserQuestion
---

# uZX Release Process

## Branching Model

| Branch | Purpose | Tag suffix |
|--------|---------|------------|
| `develop` | Alpha/dev pre-releases | `-alpha`, `-dev`, etc. |
| `main` | Stable releases only | none (e.g. `v0.5.0`) |

`main` is protected — requires PRs and passing CI. Never push directly to `main`.

## Version File

Version is defined in `versions.cmake` as `PROJECT_CORE_VERSION`. This value propagates to all app targets (Studio, Tuning, Player).

## Release Flows

### Alpha/Pre-release (from `develop`)

No PR to `main` required. Steps:

1. Ensure on `develop` branch
2. Bump `PROJECT_CORE_VERSION` in `versions.cmake`
3. Commit: `git commit -m "bump version to X.Y.Z"`
4. Push: `git push origin develop`
5. Tag: `git tag vX.Y.Z-alpha` (or `-dev`, `-beta`, etc.)
6. Push tag: `git push origin vX.Y.Z-alpha`

### Stable Release (from `main`)

Requires PR from `develop` to `main`. Steps:

1. Ensure on `develop` branch
2. Bump `PROJECT_CORE_VERSION` in `versions.cmake`
3. Commit and push to `develop`
4. Create PR: `gh pr create --base main --head develop --title "Release vX.Y.Z" --body "Stable release vX.Y.Z."`
5. Wait for CI, merge the PR
6. Tag merged commit: `git fetch origin main && git tag vX.Y.Z origin/main`
7. Push tag: `git push origin vX.Y.Z`

## CI Pipeline (`.github/workflows/ci.yml`)

### Branch push (`main`/`develop`)
- Runs `build` job (Debug, tests only) on macOS, Linux, Windows
- Uses ccache (macOS/Linux) and sccache (Windows)

### Tag push (`v*`) — Release Pipeline
Triggers regardless of which branch the tag points to.

1. **`create-release`** — Creates a draft GitHub Release
   - Sets `prerelease: true` if tag contains `-alpha` or `-beta`
   - Generates release notes automatically
2. **`release-macos`** / **`release-linux`** / **`release-windows`** — Build + test + package + upload (in parallel)
   - Builds Release config for targets: `uZX`, `uZXPlayer`, `uZXTests`
   - Runs tests
   - Packages and uploads artifacts:
     - macOS: `uZX-{ver}-macOS.zip`, `uZXPlayer-{ver}-macOS.zip`
     - Linux: `uZX-{ver}-linux-x86_64.tar.gz`, `uZXPlayer-{ver}-linux-x86_64.tar.gz`
     - Windows: `uZX-{ver}-windows-x64.zip`, `uZXPlayer-{ver}-windows-x64.zip`
3. **`publish-release`** — After all platforms finish:
   - Downloads all artifacts, generates `SHA256SUMS.txt`
   - Publishes the release (removes draft status)

### Concurrency
- Branch runs are auto-cancelled when superseded
- Tag (release) runs are never cancelled

## Instructions for Claude

When the user asks to create a release:

1. **Verify branch**: Must be on `develop` for alpha/dev, or have a clean `develop` for stable.
2. **Check current version**: Read `versions.cmake` to know the current version.
3. **Ask if needed**: If the user doesn't specify a version or tag suffix, ask.
4. **Bump version**: Edit `versions.cmake` if the version needs changing.
5. **Commit and push**: Stage, commit with message `bump version to X.Y.Z`, push to the correct branch.
6. **Tag and push**: Create the tag and push it. This triggers the CI release pipeline.
7. **For stable releases**: Create the PR to `main` first, wait for merge, then tag `origin/main`.

Always confirm before pushing tags or creating PRs — these are visible actions that trigger CI pipelines.

### Checking release status
```bash
gh run list --workflow=ci.yml --limit=5
gh run view <run-id>
gh release view <tag>
```
