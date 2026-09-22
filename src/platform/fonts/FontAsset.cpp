#include "FontAsset.h"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <span>

#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

namespace platform::fonts {
namespace {

constexpr size_t maxBitmapBytes = 1024 * 1024;
constexpr size_t maxGlyphs = 8192;

void* allocate(size_t size) {
#ifdef ARDUINO
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return std::malloc(size);
#endif
}

uint16_t little16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0] | (uint16_t{bytes[1]} << 8));
}

uint32_t little32(const uint8_t* bytes) {
    return little16(bytes) | (uint32_t{little16(bytes + 2)} << 16);
}

bool read(std::FILE* file, void* destination, size_t size) {
    return std::fread(destination, 1, size, file) == size;
}

bool inflate(std::span<const uint8_t> source, std::span<uint8_t> destination) {
    mz_stream stream{};
    stream.next_in = source.data();
    stream.avail_in = static_cast<unsigned>(source.size());
    stream.next_out = destination.data();
    stream.avail_out = static_cast<unsigned>(destination.size());
    if (mz_inflateInit(&stream) != MZ_OK) return false;
    const int result = mz_inflate(&stream, MZ_FINISH);
    const bool valid =
        result == MZ_STREAM_END && stream.total_in == source.size() && stream.total_out == destination.size();
    mz_inflateEnd(&stream);
    return valid;
}

bool validGlyph(std::span<const uint8_t> data, size_t pixels, uint8_t riceBits) {
    if (riceBits == 0) return data.size() == (pixels + 7) / 8;
    size_t position = 0;
    const auto bit = [&]() {
        const size_t index = position++;
        return (data[index / 8] >> (7 - index % 8)) & 1;
    };
    while (pixels > 0) {
        size_t quotient = 0;
        while (true) {
            if (position >= data.size() * 8) return false;
            if (!bit()) break;
            if (++quotient > (pixels >> riceBits)) return false;
        }
        if (position + riceBits > data.size() * 8) return false;
        size_t remainder = 0;
        for (uint8_t i = 0; i < riceBits; ++i) remainder = (remainder << 1) | bit();
        const size_t run = (quotient << riceBits) | remainder;
        if (run > pixels) return false;
        pixels -= run;
    }
    return (position + 7) / 8 == data.size();
}

bool validBitmap(const freeink::ui::BitmapFont& font, size_t bytes) {
    for (size_t i = 0; i < font.glyphCount; ++i) {
        const auto& glyph = font.glyphs[i];
        const size_t end = i + 1 < font.glyphCount ? font.glyphs[i + 1].bitmapOffset : bytes;
        if (glyph.bitmapOffset > end || end > bytes) return false;
        if (!validGlyph({font.bitmap + glyph.bitmapOffset, end - glyph.bitmapOffset},
                        size_t{glyph.width} * glyph.height, font.riceBits))
            return false;
    }
    return font.glyphs[0].bitmapOffset == 0;
}

}  // namespace

bool FontAsset::_readTables(std::FILE* file, uint8_t version) {
    auto& font = _font;
    const auto count = font.glyphCount;
    if (version == 3) {
        if (!read(file, &font.riceBits, 1) || (font.riceBits != 2 && font.riceBits != 3)) return false;
        _codepoints.reset(static_cast<uint16_t*>(allocate(count * sizeof(uint16_t))));
        if (!_codepoints) return false;
        font.codepoints = _codepoints.get();
        for (size_t i = 0; i < count; ++i) {
            uint8_t code[2];
            if (!read(file, code, 2)) return false;
            _codepoints.get()[i] = little16(code);
            if (i && font.codepoints[i] <= font.codepoints[i - 1]) return false;
        }
        if (font.codepoints[0] != font.first || font.codepoints[count - 1] != font.last) return false;
    }
    for (size_t i = 0; i < count; ++i) {
        uint8_t record[9]{};
        const size_t offsetBytes = version == 1 ? 2 : 4;
        if (!read(file, record, offsetBytes + 5)) return false;
        auto& glyph = _glyphs.get()[i];
        glyph = {version == 1 ? little16(record) : little32(record),
                 record[offsetBytes],
                 record[offsetBytes + 1],
                 record[offsetBytes + 2],
                 static_cast<int8_t>(record[offsetBytes + 3]),
                 static_cast<int8_t>(record[offsetBytes + 4])};
        font.maxWidth = std::max(font.maxWidth, glyph.width);
        font.maxHeight = std::max(font.maxHeight, glyph.height);
    }
    return true;
}

bool FontAsset::load(const char* path) {
    const std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(path, "rb"), &std::fclose);
    std::array<uint8_t, 26> header{};
    if (!file || !read(file.get(), header.data(), header.size())) return false;
    const uint8_t version = header[8];
    const size_t bitmapBytes = little32(header.data() + 16);
    const size_t compressedBytes = little32(header.data() + 20);
    const uint16_t count = little16(header.data() + 24);
    const uint16_t first = little16(header.data() + 10), last = little16(header.data() + 12);
    if (std::memcmp(header.data(), "DRFONT1\0", 8) != 0 || version < 1 || version > 3 || header[9] != 1 ||
        !header[14] || header[15] > header[14] || first > last || !count || count > maxGlyphs || !bitmapBytes ||
        bitmapBytes > maxBitmapBytes || !compressedBytes || compressedBytes > maxBitmapBytes ||
        (version != 3 && count != uint32_t{last} - first + 1))
        return false;

    FontAsset loaded;
    loaded._bitmap.reset(static_cast<uint8_t*>(allocate(bitmapBytes)));
    loaded._glyphs.reset(static_cast<freeink::ui::FontGlyph*>(allocate(count * sizeof(freeink::ui::FontGlyph))));
    if (!loaded._bitmap || !loaded._glyphs) return false;
    auto& font = loaded._font;
    font = {
        loaded._bitmap.get(), loaded._glyphs.get(), first, last, header[14], header[15], 0, 0, 1, nullptr, count, 0};
    if (!loaded._readTables(file.get(), version)) return false;
    FontMemory<uint8_t> compressed(static_cast<uint8_t*>(allocate(compressedBytes)));
    if (!compressed || !read(file.get(), compressed.get(), compressedBytes) || std::fgetc(file.get()) != EOF ||
        std::ferror(file.get()) || !inflate({compressed.get(), compressedBytes}, {loaded._bitmap.get(), bitmapBytes}) ||
        !validBitmap(font, bitmapBytes))
        return false;
    *this = std::move(loaded);
    return true;
}

}  // namespace platform::fonts
