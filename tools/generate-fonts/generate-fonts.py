#!/usr/bin/env python3
"""Generate Dayring's 1bpp UI fonts. Requires Pillow and fonttools."""
from pathlib import Path
import math
import struct
import zlib

from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[2]
FILESYSTEM_OUTPUT = ROOT / "data" / "fonts"
SPECS = [
    ("RobotoS", "roboto/Roboto-Regular.ttf", 23),
    ("RobotoM", "roboto/Roboto-Regular.ttf", 28),
    ("RobotoL", "roboto/Roboto-Medium.ttf", 32),
    ("RobotoXL", "roboto/Roboto-Medium.ttf", 40),
    ("Roboto2XL", "roboto/Roboto-Medium.ttf", 46),
    ("NDot2XL", "ndot-57/Ndot57-Regular.ttf", 46),
    ("NDot4XL", "ndot-57/Ndot57-Regular.ttf", 80),
    ("NDot120", "ndot-57/Ndot57-Regular.ttf", 120),
]


CJK_SIZES = {"RobotoS": 22, "RobotoM": 26, "RobotoL": 30, "RobotoXL": 38, "Roboto2XL": 43}

def gb2312_characters():
    """Return GB2312's 6,763 ideographs and common punctuation."""
    chars = []
    for lead in range(0xB0, 0xF8):
        for trail in range(0xA1, 0xFF):
            try:
                chars.append(bytes((lead, trail)).decode("gb2312"))
            except UnicodeDecodeError:
                pass
    if len(chars) != 6763:
        raise ValueError("GB2312 must contain 6,763 ideographs")
    for lead in (0xA1, 0xA3):
        for trail in range(0xA1, 0xFF):
            try:
                chars.append(bytes((lead, trail)).decode("gb2312"))
            except UnicodeDecodeError:
                pass
    return set(chars) | set("「」『』〔〕〖〗〘〙〚〛—·〇")


def rasterize(font, character):
    left, top, right, bottom = font.getbbox(character)
    width, height = right - left, bottom - top
    image = Image.new("1", (width, height))
    if width and height:
        ImageDraw.Draw(image).text((-left, -top), character, font=font, fill=1)
    pixels = list(image.get_flattened_data())
    data = bytearray()
    for start in range(0, len(pixels), 8):
        byte = 0
        for bit, pixel in enumerate(pixels[start:start + 8]):
            byte |= bool(pixel) << (7 - bit)
        data.append(byte)
    return data, width, height, left, top


def compress_glyph(data, pixel_count, rice_bits):
    bits = []
    def emit_run(length):
        bits.extend([1] * (length >> rice_bits))
        bits.append(0)
        bits.extend((length >> shift) & 1 for shift in reversed(range(rice_bits)))
    previous, length = 0, 0
    for index in range(pixel_count):
        bit = (data[index // 8] >> (7 - index % 8)) & 1
        if bit == previous:
            length += 1
        else:
            emit_run(length)
            previous, length = bit, 1
    if pixel_count:
        emit_run(length)
    return bytes(sum(bit << (7 - j) for j, bit in enumerate(bits[i:i + 8]))
                 for i in range(0, len(bits), 8))


def generate(name, source, size):
    path = ROOT / "fonts" / source
    font = ImageFont.truetype(str(path), size)
    with TTFont(path) as ttf:
        supported = ttf.getBestCmap()
    cjk_font = None
    cjk_supported = set()
    if name in CJK_SIZES:
        cjk_path = ROOT / "fonts" / "source-han-sans/SourceHanSansSC-Medium.ttf"
        cjk_font = ImageFont.truetype(str(cjk_path), CJK_SIZES[name])
        with TTFont(cjk_path) as cjk_ttf:
            cjk_supported = set(cjk_ttf.getBestCmap())
        characters = set(map(chr, range(32, 127))) | gb2312_characters()
        first, last = ord(min(characters)), ord(max(characters))
    else:
        characters = set("0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ" if size == 120 else map(chr, range(32, 127)))
        first, last = ord(min(characters)), ord(max(characters))
    missing = [ch for ch in characters if ord(ch) not in (supported if ord(ch) < 127 or not cjk_font else cjk_supported)]
    if missing:
        raise ValueError(f"{source}: missing characters {missing!r}")
    ascent, descent = font.getmetrics()
    if name == "RobotoL":
        # Preserve the established L line box and Latin baseline.
        ascent, descent = 38, 10
    cell = math.ceil(max(max(font.getlength(ch), font.getbbox(ch)[2] - font.getbbox(ch)[0]) for ch in characters if ord(ch) in supported))
    rice_bits = (3 if name == "Roboto2XL" else 2) if cjk_font else 0
    codes = sorted(map(ord, characters)) if cjk_font else list(range(first, last + 1))
    bitmap, glyphs = bytearray(), []
    for code in codes:
        character = chr(code)
        offset = len(bitmap)
        if character not in characters:
            # BitmapFont requires a contiguous table; reserved entries carry no ink.
            glyphs.append((offset, 0, 0, 0 if cjk_font else cell, 0, 0))
            continue
        glyph_font = cjk_font if cjk_font and ord(character) >= 127 else font
        data, width, height, left, top = rasterize(glyph_font, character)
        advance = round(glyph_font.getlength(character))
        if size == 120:
            # Keep the separator at its natural width; letters and digits share a cell.
            advance = max(advance, width) if character == ":" else cell
            left = (advance - width) // 2
        glyph = (offset, width, height, advance, left, top - glyph_font.getmetrics()[0])
        limits = ((0, 0xFFFFFFFF), (0, 255), (0, 255), (0, 255), (-128, 127), (-128, 127))
        if not all(lo <= value <= hi for value, (lo, hi) in zip(glyph, limits)):
            raise ValueError(f"{name}: glyph {character!r} exceeds SDK metrics: {glyph}")
        bitmap.extend(compress_glyph(data, width * height, rice_bits) if cjk_font else data)
        glyphs.append(glyph)
    if not 0 < ascent + descent <= 255 or not 0 <= ascent <= 255:
        raise ValueError(f"{name}: line metrics exceed SDK limits")
    # Filesystem format: magic, version, metrics, glyph count, uncompressed and
    # compressed bitmap lengths, glyph table, then zlib-compressed bitmap.
    # All integers are little-endian and the format contains no native pointers.
    filesystem_name = {
        "RobotoS": "sans-s", "RobotoM": "sans-m", "RobotoL": "sans-l", "RobotoXL": "sans-xl",
        "Roboto2XL": "sans-2xl", "NDot2XL": "ndot-2xl", "NDot4XL": "ndot-4xl",
        "NDot120": "ndot-120",
    }[name]
    compressed_bitmap = zlib.compress(bytes(bitmap), level=9)
    version = 3 if cjk_font else 1
    glyph_bytes = b"".join(struct.pack("<IBBBbb" if version == 3 else "<HBBBbb", *glyph) for glyph in glyphs)
    header = struct.pack("<8sBBHHBBII", b"DRFONT1\0", version, 1, first, last,
                         ascent + descent, ascent, len(bitmap), len(compressed_bitmap))
    filesystem_destination = FILESYSTEM_OUTPUT / f"{filesystem_name}.bin"
    filesystem_destination.parent.mkdir(parents=True, exist_ok=True)
    sparse_header = (struct.pack("<B", rice_bits) + struct.pack(f"<{len(codes)}H", *codes)) if cjk_font else b""
    payload = header + struct.pack("<H", len(glyphs)) + sparse_header + glyph_bytes + compressed_bitmap
    if not filesystem_destination.exists() or filesystem_destination.read_bytes() != payload:
        filesystem_destination.write_bytes(payload)
    print(f"{name}: {len(characters)} characters, {len(bitmap)} bitmap bytes, line height {ascent + descent}"
          + (f", alphanumeric advance {cell}px, colon advance {glyphs[ord(':') - first][3]}px, "
             f"HH:MM width {cell * 4 + glyphs[ord(':') - first][3]}px" if size == 120 else ""))


if __name__ == "__main__":
    for spec in SPECS:
        generate(*spec)

    # Retire the generator-owned C++ bitmap tables after producing all FATFS assets.
    legacy = ROOT / "src/platform/fonts/generated"
    for name in ("RobotoS", "RobotoM", "RobotoL", "RobotoXL", "Roboto2XL",
                 "SansS", "SansM", "SansL", "SansXL", "Sans2XL", "NDot2XL", "NDot4XL", "NDot120"):
        (legacy / f"{name}.h").unlink(missing_ok=True)
