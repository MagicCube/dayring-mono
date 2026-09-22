"""Check FATFS upload boundaries and session lifetime without touching hardware."""
import importlib.util
import io
from contextlib import redirect_stdout, redirect_stderr
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch, MagicMock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("sync_fs", ROOT / "tools/sync-fs.py")
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


class SyncFsTest(unittest.TestCase):
    def test_single_session_uncompressed_and_exact_partition_coverage(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "fatfs.bin"
            data = bytes(range(256)) * (sync.CHUNK_SIZE * 2 // 256 + 16)
            image.write_bytes(data)
            paths = []

            def inspect(command, count):
                self.assertEqual(count, 3)
                self.assertIn("--no-compress", command)
                self.assertNotIn("--compress", command)
                self.assertEqual(command.count("write-flash"), 1)
                self.assertEqual(command.count("--before"), 1)
                self.assertEqual(command.count("--after"), 1)
                self.assertEqual(command[command.index("--port") + 1], "/dev/test")
                self.assertEqual(command[command.index("--baud") + 1], "460800")
                pairs = command[command.index("detect") + 1:]
                payload = bytearray()
                for address, filename in zip(pairs[::2], pairs[1::2]):
                    self.assertEqual(int(address, 0), 0x410000 + len(payload))
                    path = Path(filename)
                    paths.append(path)
                    chunk = path.read_bytes()
                    self.assertLessEqual(len(chunk), sync.CHUNK_SIZE)
                    payload.extend(chunk)
                self.assertEqual(payload, data)
                self.assertEqual(len(paths), 3)

            with patch.object(sync, "run_upload", side_effect=inspect) as run:
                sync.upload(image, 0x410000, Path("esptool"), "/dev/test", 460800, sync.CHUNK_SIZE)
                run.assert_called_once()
            self.assertTrue(all(not path.exists() for path in paths))

    def test_failure_propagates_without_further_upload_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "fatfs.bin"
            image.write_bytes(b"\xff" * sync.SECTOR)
            with patch.object(sync, "run_upload", side_effect=subprocess.CalledProcessError(2, "esptool")) as run:
                with self.assertRaises(subprocess.CalledProcessError):
                    sync.upload(image, 0x410000, Path("esptool"), None, 460800, sync.CHUNK_SIZE)
                run.assert_called_once()

    def test_quiet_output_marks_only_verified_chunks_done(self):
        process = MagicMock()
        process.__enter__.return_value = process
        process.stdout = iter(["Connecting...\n", "Wrote 262144 bytes\n",
                               "Hash of data verified.\n", "Hash of data verified.\n"])
        process.wait.return_value = 0
        output = io.StringIO()
        with patch.object(sync.subprocess, "Popen", return_value=process) as popen, redirect_stdout(output):
            sync.run_upload(["esptool"], 2)
        popen.assert_called_once()
        self.assertEqual(output.getvalue(), "Uploading 1/2... done\nUploading 2/2... done\n")

    def test_failure_shows_details_without_false_done(self):
        for exit_code, lines in [(2, ["Connecting...\n", "The chip stopped responding.\n"]),
                                 (0, ["Wrote 262144 bytes\n"])]:
            process = MagicMock()
            process.__enter__.return_value = process
            process.stdout = iter(lines)
            process.wait.return_value = exit_code
            output, errors = io.StringIO(), io.StringIO()
            with patch.object(sync.subprocess, "Popen", return_value=process), \
                    redirect_stdout(output), redirect_stderr(errors):
                with self.assertRaises(subprocess.CalledProcessError):
                    sync.run_upload(["esptool"], 1)
            self.assertEqual(output.getvalue(), "Uploading 1/1... failed\n")
            self.assertIn(lines[-1], errors.getvalue())

    def test_invalid_image_or_alignment_never_contacts_device(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "fatfs.bin"
            for size, offset, chunk in [(0, 0x410000, 4096), (1, 0x410000, 4096),
                                        (4096, 0x410001, 4096), (4096, 0x410000, 1)]:
                image.write_bytes(b"\x00" * size)
                with patch.object(sync.subprocess, "run") as run:
                    with self.assertRaises(ValueError):
                        sync.upload(image, offset, Path("esptool"), None, 460800, chunk)
                    run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
