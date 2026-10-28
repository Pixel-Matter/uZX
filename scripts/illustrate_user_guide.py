#!/usr/bin/env python3
"""Capture the guide's six illustrations with Peekaboo, then update its marked sections."""

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
GUIDE = ROOT / "docs/user-guide.md"
EXAMPLE = ROOT / "docs/examples/Guide melody.uzx"
FIGURES = {
    "player-overview": ("Player transport and timeline", "The transport sits above the timeline; the AY controls are on the right."),
    "player-ay-panel": ("Player AY side panel", "Chip type and clock, stereo controls, and the three channel strips."),
    "studio-layout": ("Studio layout with a selected PSG clip", "Track headers are on the left, clips above, and the selected clip's parameter editor below."),
    "studio-psg-editor": ("PSG parameter editor", "Choose a register parameter in the list to inspect its values against the timeline grid."),
    "studio-devices": ("Studio track device chain", "Selecting the track reveals its devices, including the AY chip emulator."),
    "tuning-layout": ("Tuning tables, controls, and note grid", "Choose a table on the left; pitch and preview controls sit above the note grid, with CSV export below."),
}


def update_guide(text):
    for name, (alt, caption) in FIGURES.items():
        start, end = f"<!-- guide-figure:{name} -->", f"<!-- /guide-figure:{name} -->"
        pattern = re.escape(start) + r".*?" + re.escape(end)
        if len(re.findall(pattern, text, flags=re.DOTALL)) != 1:
            raise ValueError(f"Expected exactly one complete illustration marker for {name}")
        block = f"{start}\n![{alt}](images/user-guide/{name}.png)\n\n{caption}\n{end}"
        text = re.sub(pattern, lambda _: block, text, flags=re.DOTALL)
    return text


def png_size(path):
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Not a PNG screenshot: {path}")
    size = struct.unpack(">II", header[16:24])
    if min(size) < 100:
        raise ValueError(f"Screenshot is too small: {path}: {size}")
    return size


def peek(*args, required=True):
    result = subprocess.run(
        [str(ROOT / "scripts/peekaboo.sh"), *map(str, args), "--json"],
        capture_output=True, text=True, timeout=35,
    )
    try:
        data = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(result.stderr or result.stdout or "Peekaboo returned no JSON") from error
    if required and (result.returncode or not data.get("success")):
        raise RuntimeError(f"Peekaboo {' '.join(map(str, args[:2]))}: {data.get('error', data)}")
    return data


def wait_window(pid, title=None):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        result = peek("window", "list", "--pid", pid, required=False)
        for window in (result.get("data") or {}).get("windows", []):
            if window.get("is_on_screen") and (title is None or window.get("window_title") == title):
                return window
        time.sleep(0.2)
    raise RuntimeError(f"Window did not become ready: PID {pid}, title {title!r}")


def resize(pid, window, width, height):
    return peek("window", "set-bounds", "--pid", pid, "--window-id", window,
                "--x", 60, "--y", 90, "--width", width, "--height", height)


def observe(pid, window, path, roi=None):
    args = ["see", "--pid", pid, "--window-id", window, "--path", path, "--fresh"]
    if roi:
        args.extend(("--roi", ",".join(map(str, roi))))
    result = peek(*args)
    png_size(path)
    return result["data"]


def element(scene, label, role=None):
    matches = [e for e in scene["ui_elements"] if e.get("label") == label and (role is None or e.get("role") == role)]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one visible control named {label!r}, found {len(matches)}")
    return matches[0]


def click_at(pid, window, scene, x, y):
    peek("app", "focus", "--pid", pid, "--foreground")
    # Native clicks fire JUCE's listBoxItemClicked; AXPress only changes row selection.
    peek("click", "--pid", pid, "--window-id", window, "--snapshot", scene["snapshot_id"],
         "--at", f"{round(x)},{round(y)}", "--foreground")


def menu(pid, path):
    peek("menu", "click", "--pid", pid, "--path", path)


def replace_bytes(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    mode = path.stat().st_mode & 0o777 if path.exists() else 0o600
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(data)
    try:
        temporary.chmod(mode)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


@contextmanager
def demo_startup(settings, project):
    backup = settings.with_suffix(settings.suffix + ".guide-backup")
    if backup.exists():
        raise RuntimeError(f"Restore the previous capture's settings backup before continuing: {backup}")
    original = settings.read_bytes() if settings.exists() else None
    properties = ET.fromstring(original) if original is not None else ET.Element("PROPERTIES")
    recent = properties.find("VALUE[@name='recentEdits']")
    if recent is None:
        recent = ET.SubElement(properties, "VALUE", {"name": "recentEdits"})
    recent.set("val", str(project))
    if original is not None:
        replace_bytes(backup, original)
    try:
        replace_bytes(settings, ET.tostring(properties, encoding="utf-8", xml_declaration=True))
        yield
    finally:
        if original is None:
            settings.unlink(missing_ok=True)
        else:
            replace_bytes(settings, original)
        backup.unlink(missing_ok=True)


@contextmanager
def application(build, target, work):
    name = "μZX Player" if target == "uZXPlayer" else "μZX Studio"
    bundle = build / f"src/{target}_artefacts/Debug/{name}.app"
    info = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
    project_dir = work / target
    project_dir.mkdir()
    project = project_dir / EXAMPLE.name
    shutil.copyfile(EXAMPLE, project)
    command = [str(bundle / "Contents/MacOS" / info["CFBundleExecutable"])]
    settings = Path.home() / f"Library/Application Support/{name}/{name}.settings"
    with demo_startup(settings, project), (work / f"{target}.log").open("w") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        try:
            title = f"{project.stem} — {name}" if target == "uZXPlayer" else project.stem
            window = wait_window(process.pid, title)
            yield process.pid, window["window_id"]
        finally:
            if process.poll() is None:
                try:
                    peek("app", "quit", "--pid", process.pid, required=False)
                    process.wait(timeout=3)
                except (OSError, RuntimeError, subprocess.SubprocessError):
                    # These processes belong to this run and only the scratch project is edited.
                    process.terminate()
                    process.wait(timeout=5)
            print(f"Closed {target} capture session", flush=True)


def capture_figures(build, work):
    with application(build, "uZXPlayer", work) as (pid, window):
        resize(pid, window, 1280, 650)
        menu(pid, "View > Zoom to project")
        scene = observe(pid, window, work / "player-overview.png")
        if EXAMPLE.stem not in scene["window_title"]:
            raise RuntimeError("Player did not load the example project")
        width, height = scene["coordinate_context"]["delivered_image_size"]
        # shortcut: PlayerDocument has a fixed 240-point AY panel and 40-point transport;
        # update this crop if that layout changes.
        observe(pid, window, work / "player-ay-panel.png", (width - 240, 72, 240, height - 72))
        print("Captured Player transport/timeline and AY panel", flush=True)

    with application(build, "uZX", work) as (pid, window):
        resize(pid, window, 1280, 800)
        menu(pid, "View > Zoom to project")
        scene = observe(pid, window, work / "studio-before.png")
        track = element(scene, EXAMPLE.stem, "textField")
        origin = scene["coordinate_context"]["logical_bounds"][0]
        b = track["bounds"]
        click_at(pid, window, scene, b["x"] + b["width"] + 40 - origin[0], b["y"] + 60 - origin[1])
        scene = observe(pid, window, work / "studio-selected.png")
        # JUCE exposes painted list items as Row N, not their visible parameter names.
        b = element(scene, "Row 1")["bounds"]
        click_at(pid, window, scene, b["x"] + b["width"] / 2 - origin[0], b["y"] + b["height"] / 2 - origin[1])
        scene = observe(pid, window, work / "studio-layout.png")
        width, height = scene["coordinate_context"]["delivered_image_size"]
        top = round(element(scene, "Clip Parameters")["bounds"]["y"] - origin[1] - 8)
        observe(pid, window, work / "studio-psg-editor.png", (0, top, width, height - top))
        scene = observe(pid, window, work / "studio-before-devices.png")
        b = element(scene, EXAMPLE.stem, "textField")["bounds"]
        click_at(pid, window, scene, b["x"] + b["width"] / 2 - origin[0], b["y"] + 90 - origin[1])
        # shortcut: the demo has one AY device; widen the crop when adding more devices.
        scene = observe(pid, window, work / "studio-devices.png", (0, top, min(width, 360), height - top))
        element(scene, "AY", "button")
        print("Captured Studio layout, PSG editor, and track devices", flush=True)

        menu(pid, "Settings > Tuning Tables")
        tuning = wait_window(pid, "Tuning")
        resize(pid, tuning["window_id"], 1200, 780)
        scene = observe(pid, tuning["window_id"], work / "tuning-layout.png")
        element(scene, "Export to CSV")
        print("Captured Tuning tables, controls, and grid", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build", help="CMake Debug build containing uZX and uZXPlayer")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    original = GUIDE.read_text()
    updated = update_guide(original)
    for target, name in (("uZX", "μZX Studio.app"), ("uZXPlayer", "μZX Player.app")):
        if not (build / f"src/{target}_artefacts/Debug" / name / "Contents/Info.plist").exists():
            parser.error(f"Build {target} first: cmake --build {build} --target {target} --parallel")
    apps = peek("app", "list")["data"]["apps"]
    if any(a.get("bundle_id") in ("org.pixelmatter.uZX", "org.pixelmatter.uZXPlayer") for a in apps):
        parser.error("Close Studio and Player first; capture must not touch an existing session.")
    cache = ROOT / ".cache"
    cache.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="guide-figures-", dir=cache) as temporary:
        work = Path(temporary)
        capture_figures(build, work)
        for name in FIGURES:
            png_size(work / f"{name}.png")
        if GUIDE.read_text() != original:
            raise RuntimeError("The guide changed during capture; leaving its text untouched")
        destination = ROOT / "docs/images/user-guide"
        destination.mkdir(parents=True, exist_ok=True)
        for name in FIGURES:
            (work / f"{name}.png").replace(destination / f"{name}.png")
        replacement = work / "user-guide.md"
        replacement.write_text(updated)
        replacement.replace(GUIDE)
    print("Updated docs/user-guide.md with six illustrations", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
