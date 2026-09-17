"""Encode the exact grayscale framebuffer without image-library dependencies."""

import os
from pathlib import Path
import struct
import tempfile
import zlib


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def encode(width: int, height: int, pixels: bytes) -> bytes:
    if width <= 0 or height <= 0 or len(pixels) != width * height:
        raise ValueError("Invalid grayscale framebuffer dimensions or payload length")
    scanlines = b"".join(b"\0" + pixels[y * width:(y + 1) * width] for y in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(scanlines, 9)) + chunk(b"IEND", b""))


def publish(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix=".preview-", suffix=".png", dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
