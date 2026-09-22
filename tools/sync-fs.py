#!/usr/bin/env python3
"""Upload FATFS in one esptool session with bounded, uncompressed writes."""
from __future__ import annotations

import argparse
from collections import deque
import os
import sys
import subprocess
import tempfile
from pathlib import Path

CHUNK_SIZE = 256 * 1024
SECTOR = 4096


def run_upload(command: list[str], count: int) -> None:
    environment = dict(os.environ, PYTHONUNBUFFERED="1", NO_COLOR="1", TERM="dumb")
    recent = deque(maxlen=80)
    completed = 0
    print(f"Uploading 1/{count}...", end=" ", flush=True)
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace", bufsize=1,
                          env=environment) as process:
        for line in process.stdout:
            recent.append(line)
            if line.strip() == "Hash of data verified." and completed < count:
                completed += 1
                print("done", flush=True)
                if completed < count:
                    print(f"Uploading {completed + 1}/{count}...", end=" ", flush=True)
        code = process.wait()
    if code or completed != count:
        if completed < count:
            print("failed", flush=True)
        print("".join(recent), file=sys.stderr, end="")
        if not code:
            print(f"Missing verification: {completed}/{count} chunks confirmed.", file=sys.stderr)
        raise subprocess.CalledProcessError(code or 1, command)


def upload(image: Path, offset: int, esptool: Path, port: str | None,
           baud: int, chunk_size: int) -> None:
    if offset < 0 or offset % SECTOR or not image.is_file():
        raise ValueError("invalid filesystem image or unaligned offset")
    size = image.stat().st_size
    if not size or size % SECTOR:
        raise ValueError("filesystem image must contain complete 4096-byte sectors")
    if chunk_size <= 0 or chunk_size % SECTOR:
        raise ValueError("chunk size must be a positive multiple of 4096")
    if baud <= 0:
        raise ValueError("baud rate must be positive")

    command = [str(esptool), "--chip", "esp32s3"]
    if port:
        command += ["--port", port]
    command += ["--baud", str(baud), "--before", "default-reset", "--after", "hard-reset",
                "write-flash", "--no-progress", "--no-compress", "--flash-size", "detect"]
    with tempfile.TemporaryDirectory(prefix="dayring-fs-") as directory:
        with image.open("rb") as source:
            for index, address in enumerate(range(offset, offset + size, chunk_size)):
                chunk = Path(directory) / f"chunk-{index:02d}.bin"
                chunk.write_bytes(source.read(min(chunk_size, offset + size - address)))
                command += [hex(address), str(chunk)]
        count = (size + chunk_size - 1) // chunk_size
        # write-flash verifies each file's digest before moving to the next address.
        # One process keeps the same port/stub alive instead of booting a partial FAT image.
        run_upload(command, count)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--offset", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--esptool", required=True, type=Path)
    parser.add_argument("--port")
    parser.add_argument("--baud", default=460800, type=int)
    parser.add_argument("--chunk-size", default=CHUNK_SIZE, type=lambda value: int(value, 0))
    args = parser.parse_args()
    try:
        upload(args.image, args.offset, args.esptool, args.port, args.baud, args.chunk_size)
    except ValueError as error:
        parser.error(str(error))
    except subprocess.CalledProcessError as error:
        raise SystemExit(f"FATFS upload failed (esptool exit {error.returncode}); "
                         "the image is incomplete. Fix the connection and rerun make fs:upload.") from None


if __name__ == "__main__":
    main()
