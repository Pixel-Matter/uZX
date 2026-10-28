# Agent UX testing on macOS

Use [Peekaboo](https://github.com/openclaw/Peekaboo) through `scripts/peekaboo.sh`.
It captures native windows, exposes the accessibility tree, and drives clicks,
typing, key chords, scrolling, and dragging. The launcher downloads a pinned,
SHA-256-verified release into ignored `.cache/peekaboo/` on first use. It requires
macOS 15 or later and network access for that first download. It adds no JUCE or
application dependency and needs no AI API key for these commands.

The [JUCE WebView driver](https://github.com/mateusz28011/juce-webview-driver)
requires `juce::WebBrowserComponent`; uZX uses native components.
[Melatonin Inspector](https://github.com/sudara/melatonin_inspector) is useful for
interactive component inspection, but does not provide this agent input harness.

## Prepare

Run commands from the repository root. Desktop automation must run outside the
agent's filesystem sandbox; sandboxed permission results can be misleading.

```sh
./scripts/peekaboo.sh --version
./scripts/peekaboo.sh permissions status --all-sources --json
```

The launcher has offline failure/argument checks:
`python3 -B -m unittest discover -s tests -p test_ui_harness.py`.

Screen Recording and Accessibility must be granted to the runtime reported by
`permissions status`. Event Synthesizing is also needed for raw keyboard/mouse
events. If missing, use `./scripts/peekaboo.sh permissions grant` for the macOS
setup instructions; the agent cannot grant these permissions itself.

Build the desired application, then launch its exact bundle:

```sh
cmake --preset default
cmake --build build --target uZX --parallel
./scripts/peekaboo.sh app launch "$PWD/build/src/uZX_artefacts/Debug/μZX Studio.app" \
    --foreground --wait-for-window --json
./scripts/peekaboo.sh window list --app org.pixelmatter.uZX --json
```

Other bundle IDs are `org.pixelmatter.uZXTuning` and `org.pixelmatter.uZXPlayer`.
Use the returned PID and window ID to distinguish builds in different worktrees.
Work on a disposable document for tests that edit content.

## Observe, act, verify

Save screenshots and JSON under ignored `.cache/ux/`. Replace `PID`, `WINDOW_ID`,
`SNAPSHOT_ID`, and `ELEMENT_ID` below with values from the preceding observation.

```sh
mkdir -p .cache/ux
./scripts/peekaboo.sh see --pid PID --window-id WINDOW_ID --annotate \
    --path "$PWD/.cache/ux/before.png" --json > .cache/ux/before.json
./scripts/peekaboo.sh click --on ELEMENT_ID --snapshot SNAPSHOT_ID --json
./scripts/peekaboo.sh see --pid PID --window-id WINDOW_ID \
    --path "$PWD/.cache/ux/after.png" --json > .cache/ux/after.json
```

Inspect the saved image as well as the JSON. Take a fresh observation after each
action and use its snapshot/element IDs for the next action. A successful input
dispatch is not proof of the intended UI change; an indeterminate result must be
observed before retrying. Avoid pixel comparisons of moving meters/playheads.

For custom-painted controls missing from the accessibility tree, click at an
observed point in a fresh screenshot. Click coordinates are relative to the
captured window in logical points. Use the default 1x capture (omit `--retina`)
and check the image dimensions against the reported window bounds:

```sh
./scripts/peekaboo.sh click --at X,Y --snapshot SNAPSHOT_ID --json
./scripts/peekaboo.sh type 'test text' --snapshot SNAPSHOT_ID --clear --json
./scripts/peekaboo.sh press tab --snapshot SNAPSHOT_ID --json
./scripts/peekaboo.sh scroll --direction down --amount 3 --at X,Y --snapshot SNAPSHOT_ID --json
./scripts/peekaboo.sh drag --from X1,Y1 --to X2,Y2 --snapshot SNAPSHOT_ID --json
```

Drag coordinates are **global logical screen points**, not image pixels. Use
element IDs or the observed window bounds to calculate them. Use `--foreground`
with an explicit `--pid`/`--window-id` when testing actual shared mouse/keyboard
input; this focuses uZX and can move the user's cursor. Background actions are
the default. Native file dialogs and popup menus may be separate windows: list
and observe them before interacting. `window list` can omit floating dialogs;
`see --pid PID --window-title 'About μZX Studio' --json` can target them directly.
Floating dialogs can reject foreground coordinate clicks (`layer != 0`); use
their semantic buttons, dialog actions, or a scoped Escape key instead.
If the app exits or restarts, rediscover its PID and window instead of reusing
an old snapshot.

See `./scripts/peekaboo.sh COMMAND --help` for exact flags, especially `move`,
`menu`, `dialog`, `window`, and `verify`. The launcher also passes through `mcp`
if an agent client needs the MCP transport; global agent configuration is not
required for CLI use.

This is a live desktop harness, not a headless or cross-platform CTest suite.
Keep model/controller assertions in the existing JUCE unit tests. Custom JUCE
controls may expose incomplete accessibility metadata; use screenshot evidence
and scoped coordinates in those cases.

## Regenerate user-guide illustrations

```sh
cmake --build build --target uZX uZXPlayer --parallel
python3 -B scripts/illustrate_user_guide.py
```

Pass `--build-dir /absolute/path/to/build` to use another Debug build. Run outside
the sandbox with Studio and Player closed; the generator refuses to reuse an
existing app session. It loads scratch copies of `docs/examples/Guide melody.uzx`,
captures the six figures, closes each test app in a `finally` block, and then
updates the marked sections of `docs/user-guide.md`. During each session it
temporarily sets that app's `recentEdits` to the scratch project so
startup loads the demo directly. After the process closes, it restores the
original settings file byte for byte, including recent files and window bounds.
A recovery copy (`.settings.guide-backup`) is kept alongside each original until
restoration succeeds. If the generator is forcibly killed, close the affected
app and restore that copy over its `.settings` file before running again.
Capture does not start playback or overwrite the example project.
Studio selection uses foreground mouse input because JUCE's painted parameter
list does not invoke its click handler through Accessibility alone. Avoid changing
focus during the brief capture; the driver refuses input when its target loses focus.

The figures cover Player's transport/timeline and AY panel, Studio's layout,
PSG parameter editor and device chain, and the Tuning table controls/grid.
These are the spatial parts of the guide; shortcut and file-format tables remain
text. Screenshots go into `docs/images/user-guide/`. The existing Player GIF is
kept as a playback demonstration. Guide text is only updated after every capture
succeeds, and repeated runs replace the same generated blocks and image files.

Run the launcher and guide-generation checks without opening any windows:

```sh
python3 -B -m unittest discover -s tests -p 'test_*.py'
```
