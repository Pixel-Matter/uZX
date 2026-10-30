"""Offline checks for the launcher: python3 -B -m unittest discover -s tests -p 'test_ui_harness.py'."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


LAUNCHER = Path(__file__).resolve().parents[1] / "scripts" / "peekaboo.sh"


class UIHarnessTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="uzx ui test ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "scripts").mkdir()
        self.launcher = self.root / "scripts" / "peekaboo.sh"
        shutil.copy2(LAUNCHER, self.launcher)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.env = {**os.environ, "PATH": f"{self.bin}:/usr/bin:/bin"}
        self.stub("uname", 'case "$1" in -s) echo Darwin;; -m) echo arm64;; esac')
        version = re.search(r"^version=(.+)$", LAUNCHER.read_text(), re.MULTILINE)[1]
        self.cache = self.root / ".cache" / "peekaboo" / f"{version}-arm64"

    def stub(self, name, body):
        path = self.bin / name
        path.write_text(f"#!/bin/sh\nset -eu\n{body}\n")
        path.chmod(0o755)
        return path

    def run_driver(self, *args):
        return subprocess.run(
            [str(self.launcher), *args], env=self.env, text=True, capture_output=True, timeout=10
        )

    def test_cached_driver_preserves_arguments_and_exit_status(self):
        self.cache.mkdir(parents=True)
        shutil.move(self.stub("peekaboo", 'printf "<%s>\\n" "$@"; exit 23'), self.cache / "peekaboo")
        self.stub("curl", "exit 99")
        result = self.run_driver("type", "two words", "$(literal)")
        self.assertEqual(result.returncode, 23)
        self.assertEqual(result.stdout, "<type>\n<two words>\n<$(literal)>\n")

    def test_download_failure_cleans_staging(self):
        self.stub("curl", "exit 22")
        self.assertEqual(self.run_driver("--version").returncode, 22)
        self.assertEqual(list(self.cache.parent.iterdir()), [])

    def test_corrupt_download_is_never_extracted_or_executed(self):
        self.stub("curl", 'while [ "$1" != --output ]; do shift; done; printf corrupt > "$2"')
        self.stub("tar", "echo should-not-extract; exit 99")
        result = self.run_driver("--version")
        self.assertEqual(result.returncode, 1)
        self.assertNotIn("should-not-extract", result.stdout)
        self.assertEqual(list(self.cache.parent.iterdir()), [])

    def test_non_macos_fails_before_downloading(self):
        self.stub("uname", "echo Linux")
        result = self.run_driver("--version")
        self.assertEqual(result.returncode, 1)
        self.assertIn("macOS", result.stderr)
        self.assertFalse(self.cache.parent.exists())


if __name__ == "__main__":
    unittest.main()
