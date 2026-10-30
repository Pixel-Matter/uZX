"""Offline tests: python3 -B -m unittest discover -s tests -p 'test_*.py'."""

import importlib.util
from contextlib import nullcontext
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("guide_illustrations", ROOT / "scripts/illustrate_user_guide.py")
guide = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(guide)


class GuideIllustrationsTest(unittest.TestCase):
    def test_insertion_is_idempotent_and_preserves_surrounding_prose(self):
        original = guide.GUIDE.read_text()
        first = guide.update_guide(original)
        self.assertEqual(first, guide.update_guide(first))
        self.assertIn("![µZX Player](uzx-player.gif)", first)
        self.assertIn("## File Formats", first)
        for name in guide.FIGURES:
            self.assertEqual(first.count(f"](images/user-guide/{name}.png)"), 1)

    def test_missing_or_duplicate_anchor_fails_before_rewriting(self):
        original = guide.GUIDE.read_text()
        marker = "<!-- guide-figure:player-overview -->"
        for invalid in (original.replace(marker, ""), original + "\n" + original):
            with self.subTest(invalid=invalid[:30]), self.assertRaises(ValueError):
                guide.update_guide(invalid)

    def test_png_validation_rejects_error_text_and_tiny_captures(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "capture.png"
            for data in (b"permission denied", b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 1, 1)):
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    guide.png_size(path)

    def test_demo_contains_real_chip_data_without_personal_paths(self):
        root = ET.parse(guide.EXAMPLE).getroot()
        self.assertEqual(root.find("TRACK").get("name"), "Guide melody")
        self.assertIsNotNone(root.find(".//PLUGIN[@type='aychip']"))
        self.assertEqual(len(root.findall(".//PSG/FRAME")), 400)
        self.assertNotIn("/Users/", guide.EXAMPLE.read_text())
        self.assertIsNone(root.get("modifiedBy"))

    def test_track_locator_distinguishes_the_document_title(self):
        track = {"label": "Guide melody", "role": "textField"}
        scene = {"ui_elements": [{"label": "Guide melody", "role": "other"}, track]}
        self.assertIs(guide.element(scene, "Guide melody", "textField"), track)
        with self.assertRaises(RuntimeError):
            guide.element(scene, "Guide melody")

    def test_native_click_focuses_only_the_capture_process(self):
        scene = {"snapshot_id": "exact-window"}
        with patch.object(guide, "peek") as driver:
            guide.click_at(123, 7, scene, 5, 10)
        self.assertEqual(driver.call_args_list[0].args, ("app", "focus", "--pid", 123, "--foreground"))
        driver.assert_called_with("click", "--pid", 123, "--window-id", 7, "--snapshot", "exact-window",
                                  "--at", "5,10", "--foreground")

    def test_startup_preferences_are_restored_exactly_on_failure(self):
        original = b'<PROPERTIES>\n<VALUE name="recentEdits" val="/existing.uzx"/>\n</PROPERTIES>\n'
        with tempfile.TemporaryDirectory() as temporary:
            settings = Path(temporary) / "app.settings"
            settings.write_bytes(original)
            settings.chmod(0o640)
            with self.assertRaisesRegex(RuntimeError, "capture failed"):
                with guide.demo_startup(settings, Path("/demo.uzx")):
                    self.assertEqual(settings.with_suffix(".settings.guide-backup").read_bytes(), original)
                    recent = ET.parse(settings).getroot().find("VALUE[@name='recentEdits']")
                    self.assertEqual(recent.get("val"), "/demo.uzx")
                    settings.write_bytes(b"changed by the running app")
                    raise RuntimeError("capture failed")
            self.assertEqual(settings.read_bytes(), original)
            self.assertEqual(settings.stat().st_mode & 0o777, 0o640)
            self.assertFalse(settings.with_suffix(".settings.guide-backup").exists())

    def test_existing_recovery_backup_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            settings = Path(temporary) / "app.settings"
            backup = settings.with_suffix(".settings.guide-backup")
            backup.write_bytes(b"original preferences")
            with self.assertRaisesRegex(RuntimeError, "previous capture"):
                with guide.demo_startup(settings, Path("/demo.uzx")):
                    self.fail("Must not launch with an unresolved settings backup")
            self.assertEqual(backup.read_bytes(), b"original preferences")
            self.assertFalse(settings.exists())

    def test_first_run_preferences_are_removed_on_exit(self):
        with tempfile.TemporaryDirectory() as temporary:
            settings = Path(temporary) / "new app/app.settings"
            with guide.demo_startup(settings, Path("/demo.uzx")):
                self.assertTrue(settings.exists())
                self.assertEqual(ET.parse(settings).getroot().find("VALUE").get("val"), "/demo.uzx")
            self.assertFalse(settings.exists())

    def test_capture_failure_still_closes_its_process(self):
        process = Mock(pid=123)
        process.poll.return_value = None
        process.wait.side_effect = [subprocess.TimeoutExpired("test app", 3), 0]
        window = {"window_id": 7, "bounds": {"x": 0, "y": 0, "width": 1000, "height": 700}}

        with tempfile.TemporaryDirectory() as temporary:
            with patch.object(guide.subprocess, "Popen", return_value=process), \
                 patch.object(guide, "peek", return_value={"success": False}) as driver, \
                 patch.object(guide, "wait_window", return_value=window), \
                 patch.object(guide, "demo_startup", return_value=nullcontext()), \
                 patch.object(guide.plistlib, "loads", return_value={"CFBundleExecutable": "Player"}), \
                 patch.object(Path, "read_bytes", return_value=b"plist"):
                with self.assertRaisesRegex(RuntimeError, "capture failed"):
                    with guide.application(ROOT, "uZXPlayer", Path(temporary)):
                        raise RuntimeError("capture failed")
                driver.assert_any_call("app", "quit", "--pid", 123, required=False)
                process.terminate.assert_called_once()


if __name__ == "__main__":
    unittest.main()
