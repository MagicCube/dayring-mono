#!/usr/bin/env python3
"""End-to-end preview contracts; PNG decoding deliberately independent of its writer."""

from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
import build
import cli


def invoke(*arguments, status=0, cwd=ROOT, env=None):
    result = subprocess.run([str(TOOLS / "preview"), *arguments], cwd=cwd, env=env,
                            capture_output=True, text=True, timeout=240)
    if result.returncode != status:
        raise AssertionError(f"{arguments}: expected {status}, got {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result


def decode(path):
    data = Path(path).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    offset = 8
    compressed = b""
    width = height = 0
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        assert zlib.crc32(kind + payload) == crc
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            assert (depth, color, compression, filtering, interlace) == (8, 0, 0, 0, 0)
        if kind == b"IDAT":
            compressed += payload
        offset += length + 12
    raw = zlib.decompress(compressed)
    assert len(raw) == (width + 1) * height
    assert all(raw[y * (width + 1)] == 0 for y in range(height))
    return width, height, b"".join(raw[y * (width + 1) + 1:(y + 1) * (width + 1)] for y in range(height))


class PreviewTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.executable, _ = build.build()
        cls.temporary = tempfile.TemporaryDirectory(dir=build.WORK / "tmp")
        cls.directory = Path(cls.temporary.name)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def capture(self, url, *arguments):
        return json.loads(invoke("capture", url, *arguments, "--json").stdout)

    def test_help_and_shared_discovery(self):
        self.assertIn("capture", invoke("--help").stdout)
        self.assertIn("--time", invoke("capture", "--help").stdout)
        self.assertIn("article=reading|display", invoke("help", "typography").stdout)
        result = json.loads(invoke("routes", "--json").stdout)
        self.assertEqual({r["url"] for r in result["routes"]}, {
            "app://shell/", "app://shell/lock", "app://typography/", "app://calendar/", "app://test/"})
        self.assertEqual(result["aliases"]["app://home"], "app://shell/")
        for route in result["routes"]:
            capture = self.capture(route["url"])
            self.assertEqual(capture["resolved_url"], route["url"])
            self.assertEqual(decode(capture["output"])[:2], (480, 800))

    def test_frame_parity_determinism_and_defaults(self):
        url = "app://typography/?article=display"
        first = self.capture(url)
        second = self.capture(url)
        self.assertEqual(first["output"], second["output"])
        self.assertTrue(second["cache_hit"])
        self.assertEqual(Path(first["output"]).name, "typography--article-display.png")
        self.assertEqual(Path(first["output"]).parent, ROOT / ".preview")
        metadata, pixels = cli.native(self.executable, ["capture", url, "12", "34", "75", "0"])
        self.assertEqual(metadata["resolved_url"], url)
        self.assertEqual(decode(first["output"])[2], pixels)
        self.assertEqual(decode(first["output"])[2], decode(second["output"])[2])
        reading = self.capture("app://typography/?article=reading")
        self.assertNotEqual(first["output"], reading["output"])
        self.assertNotEqual(decode(reading["output"])[2], pixels)
        escaped = self.capture("app://typography/?article=%64isplay&unknown=ok#fragment")
        self.assertEqual(decode(escaped["output"])[2], pixels)
        repeated = self.capture("app://typography/?article=display&article=reading")
        self.assertEqual(decode(repeated["output"])[2], pixels)

    def test_status_clock_and_lock(self):
        base = decode(self.capture("app://shell/")["output"])[2]
        for flag in (("--time", "09:15"), ("--battery", "10"), ("--charging",)):
            pixels = decode(self.capture("app://shell/", *flag)["output"])[2]
            self.assertNotEqual(base[:480 * 36], pixels[:480 * 36])
            self.assertEqual(base[480 * 36:], pixels[480 * 36:])
        locked = decode(self.capture("app://shell/lock")["output"])[2]
        self.assertEqual(set(locked[:480 * 36]), {0})
        time = decode(self.capture("app://shell/lock", "--time", "09:15")["output"])[2]
        self.assertNotEqual(locked, time)
        home = self.capture("app://home")
        self.assertEqual(home["resolved_url"], "app://shell/")
        self.assertEqual(decode(home["output"])[2], base)

    def test_errors_preserve_output(self):
        output = self.directory / "preserved.png"
        output.write_bytes(b"keep")
        cases = [("bad", 2), ("app://missing/", 3), ("app://shell/missing", 3),
                 ("app://typography/?article=invalid", 2), ("app://typography/?a=%GG", 2),
                 ("app://home/invalid", 2), ("app://shell//lock", 2)]
        for url, status in cases:
            result = invoke("capture", url, "--output", str(output), "--json", status=status)
            self.assertFalse(json.loads(result.stdout)["ok"])
            self.assertEqual(output.read_bytes(), b"keep")
        for arguments in (("--time", "25:00"), ("--battery", "101"), ("--battery", "-1")):
            result = invoke("capture", "app://shell/", *arguments, "--json", status=2)
            self.assertEqual(json.loads(result.stdout)["error"]["code"], "invalid_arguments")
        invoke("capture", "app://shell/", "--output", str(self.directory), "--json", status=6)
        invoke("help", "missing", "--json", status=3)
        env = dict(os.environ, HOST_CXX="/nonexistent/compiler")
        result = invoke("routes", "--json", env=env, status=4)
        self.assertEqual(json.loads(result.stdout)["error"]["code"], "compiler_missing")
        invoke("--help", env=dict(os.environ, PYTHON="/nonexistent/python"), status=4)

    def test_relative_path_and_concurrent_capture(self):
        result = json.loads(invoke("capture", "app://test/", "--output", "relative.png", "--json",
                                   cwd=self.directory).stdout)
        self.assertEqual(Path(result["output"]), self.directory / "relative.png")
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(lambda _: self.capture("app://calendar/"), range(2)))
        self.assertEqual(results[0]["output"], results[1]["output"])
        self.assertEqual(decode(results[0]["output"])[:2], (480, 800))

    def test_make_entry_and_output_layout(self):
        for url, name in (("app://typography/?article=display", "typography--article-display.png"),
                          ("app://shell/lock", "shell--lock.png"),
                          ("app://typography/?article=display&unused=a%26b#end", "typography--article-display-unused-a-b.png")):
            result = subprocess.run(["make", "--no-print-directory", "preview", url], cwd=ROOT,
                                    capture_output=True, text=True, timeout=240)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(str(ROOT / ".preview" / name), result.stdout)
            self.assertEqual(decode(ROOT / ".preview" / name)[:2], (480, 800))
        first = self.capture("app://shell/")
        before = Path(first["output"]).read_bytes()
        second = self.capture("app://shell/", "--time", "09:15")
        self.assertEqual(first["output"], second["output"])
        self.assertEqual(Path(first["output"]).name, "shell.png")
        self.assertNotEqual(before, Path(second["output"]).read_bytes())
        self.assertTrue(all(path.is_file() and path.suffix == ".png" for path in (ROOT / ".preview").iterdir()))
        default = subprocess.run(["make", "--no-print-directory", "preview"], cwd=ROOT,
                                 capture_output=True, text=True, timeout=240)
        self.assertEqual(default.returncode, 0, default.stderr)
        self.assertIn(str(ROOT / ".preview/shell.png"), default.stdout)
        self.assertEqual(build.WORK, ROOT / ".cache/preview")
        self.assertTrue(self.executable.is_relative_to(build.WORK))

    def test_protocol_failure(self):
        script = self.directory / "bad-renderer"
        script.write_text("#!/bin/sh\nprintf 'invalid\\n'\n")
        script.chmod(0o755)
        with self.assertRaises(cli.PreviewError) as error:
            cli.native(script, ["routes"])
        self.assertEqual(error.exception.code, "invalid_protocol")


class BuildCacheTest(unittest.TestCase):
    def test_dependency_invalidation_and_cold_concurrency(self):
        (build.WORK / "tmp").mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=build.WORK / "tmp") as temporary:
            root = Path(temporary)
            ui = root / "sdk"
            native = root / "native"
            for directory in (root / "src/platform/fonts", root / "src/platform/hal", ui / "src", native):
                directory.mkdir(parents=True)
            (root / "src/platform/fonts/Fonts.cpp").write_text("")
            (root / "src/platform/hal/PowerManager.cpp").write_text("")
            (ui / "src/FreeInkUI.cpp").write_text("")
            (native / "Main.cpp").write_text('#include "value.h"\nint main() { return VALUE; }\n')
            dependency = native / "value.h"
            dependency.write_text("#define VALUE 0\n")
            (root / "platformio.ini").write_text('[env:papermono]\nbuild_flags = \'-DDAYRING_HOME_URL="app://test/"\'\n')
            with patch.multiple(build, ROOT=root, UI=ui, NATIVE=native, RTC=root / "rtc", WORK=root / ".cache/preview"):
                self.assertEqual(build.home_flags(), ['-DDAYRING_HOME_URL="app://test/"'])
                with ThreadPoolExecutor(max_workers=2) as pool:
                    built = list(pool.map(lambda _: build.build(), range(2)))
                first = built[0][0]
                self.assertEqual(first, built[1][0])
                self.assertEqual(subprocess.run([str(first)]).returncode, 0)
                with patch.object(build, "checked", wraps=build.checked) as commands:
                    self.assertTrue(build.build()[1])
                    self.assertFalse(any("-c" in call.args[0] or "-M" in call.args[0] for call in commands.call_args_list))
                stat = dependency.stat()
                dependency.write_text("#define VALUE 1\n")
                os.utime(dependency, ns=(stat.st_atime_ns, stat.st_mtime_ns))
                with patch.object(build, "checked", wraps=build.checked) as commands:
                    second, hit = build.build()
                    compilations = [call.args[0] for call in commands.call_args_list if "-c" in call.args[0]]
                    self.assertEqual(len(compilations), 1)
                    self.assertIn(str(native / "Main.cpp"), compilations[0])
                self.assertFalse(hit)
                self.assertNotEqual(first, second)
                self.assertEqual(subprocess.run([str(second)]).returncode, 1)
                self.assertFalse(build.build(rebuild=True)[1])
                dependency.write_text('#include "added.h"\n')
                (native / "added.h").write_text("#define VALUE 2\n")
                third, hit = build.build()
                self.assertFalse(hit)
                self.assertEqual(subprocess.run([str(third)]).returncode, 2)
                dependency.write_text("#error expected compilation failure\n")
                with self.assertRaises(cli.PreviewError) as error:
                    build.build()
                self.assertEqual(error.exception.status, 4)
                self.assertEqual(subprocess.run([str(third)]).returncode, 2)


if __name__ == "__main__":
    unittest.main()
