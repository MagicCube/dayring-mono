"""Check generated font assets against the intended source faces and coverage."""
import importlib.util
from pathlib import Path
import struct
import zlib

from PIL import ImageFont

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("font_generator", ROOT / "tools/generate-fonts/generate-fonts.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


def decode_runs(data, count, rice_bits):
    bits = iter((byte >> shift) & 1 for byte in data for shift in range(7, -1, -1))
    pixels = []
    value = 0
    while len(pixels) < count:
        quotient = 0
        while next(bits):
            quotient += 1
        remainder = 0
        for _ in range(rice_bits):
            remainder = remainder * 2 + next(bits)
        pixels.extend([value] * ((quotient << rice_bits) + remainder))
        value ^= 1
    assert len(pixels) == count
    return bytes(sum(bit << (7 - j) for j, bit in enumerate(pixels[i:i + 8]))
                 for i in range(0, len(pixels), 8))


def check_font(filename, source, latin_size, cjk_size, line_height, baseline):
    payload = (ROOT / f"data/fonts/{filename}.bin").read_bytes()
    magic, version, bpp, first, last, height, ascent, raw_length, compressed_length = struct.unpack_from(
        "<8sBBHHBBII", payload)
    assert (magic, version, bpp, height, ascent) == (b"DRFONT1\0", 3, 1, line_height, baseline)
    count, = struct.unpack_from("<H", payload, 24)
    rice_bits = payload[26]
    codes = struct.unpack_from(f"<{count}H", payload, 27)
    table_start = 27 + count * 2
    bitmap_start = table_start + count * 9
    bitmap = zlib.decompress(payload[bitmap_start:])
    assert len(bitmap) == raw_length
    assert len(payload) - bitmap_start == compressed_length
    characters = set(map(chr, range(32, 127))) | generator.gb2312_characters()
    assert list(codes) == sorted(map(ord, characters))
    assert (first, last) == (codes[0], codes[-1])
    fonts = [ImageFont.truetype(str(ROOT / f"fonts/roboto/Roboto-{source}.ttf"), latin_size),
             ImageFont.truetype(str(ROOT / "fonts/source-han-sans/SourceHanSansSC-Medium.ttf"), cjk_size)]
    level_counts = [0, 0]
    for index, code in enumerate(codes):
        character = chr(code)
        offset, width, rows, advance, left, top = struct.unpack_from("<IBBBbb", payload, table_start + index * 9)
        font = fonts[code >= 127]
        data, expected_width, expected_rows, expected_left, expected_top = generator.rasterize(font, character)
        assert (width, rows, advance, left, top) == (
            expected_width, expected_rows, round(font.getlength(character)), expected_left,
            expected_top - font.getmetrics()[0]), character
        next_offset = (struct.unpack_from("<I", payload, table_start + (index + 1) * 9)[0]
                       if index + 1 < count else len(bitmap))
        assert 0 <= offset <= next_offset <= len(bitmap)
        assert decode_runs(bitmap[offset:next_offset], width * rows, rice_bits) == data, character
        assert 0 <= ascent + top <= ascent + top + rows <= height, (filename, character)
        if 0x4E00 <= code <= 0x9FFF:
            level_counts[character.encode("gb2312")[0] >= 0xD8] += 1
    assert level_counts == [3755, 3008]
    assert set("李昕，。！？：；（）《》“”‘’…—、") <= characters
    print(f"{filename}: source-face parity, lossless compression, both GB2312 levels and punctuation passed")


for case in [("sans-s", "Regular", 23, 22, 28, 22), ("sans-m", "Regular", 28, 26, 33, 26),
             ("sans-l", "Medium", 32, 30, 48, 38), ("sans-xl", "Medium", 40, 38, 48, 38),
             ("sans-2xl", "Medium", 46, 43, 55, 43)]:
    check_font(*case)
