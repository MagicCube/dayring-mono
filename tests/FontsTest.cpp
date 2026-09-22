#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cassert>
#include <cstdio>

#include "platform/fonts/Fonts.h"

namespace {

void checkCompressedGlyphs() {
    using namespace freeink::ui;
    constexpr uint8_t raw[]{0x80, 0x60, 0, 0, 0, 0};
    constexpr uint8_t rice2[]{7, 11, 252, 0};
    constexpr uint8_t rice3[]{1, 129, 120, 0};
    constexpr FontGlyph glyphs[]{{0, 43, 1, 43, 0, 0}, {0, 43, 1, 43, 0, 0}};
    constexpr uint16_t codes[]{0x674E, 0x6615};
    constexpr uint16_t sortedCodes[]{0x6615, 0x674E};
    for (const uint8_t k : {2, 3}) {
        const BitmapFont font{k == 2 ? rice2 : rice3, glyphs, 0x6615, 0x674E, 48, 38, 43, 1, 1, sortedCodes, 2, k};
        assert(findFontGlyph(font, codes[0]) == &glyphs[1]);
        assert(findFontGlyph(font, codes[1]) == &glyphs[0]);
        assert(findFontGlyph(font, 0x6616) == nullptr);
        assert(findFontGlyph(font, 0x10000) == nullptr);
        FontBitReader reader(font, glyphs[0]);
        for (int i = 0; i < 43; ++i) {
            assert(reader.next() == static_cast<bool>((raw[i / 8] >> (7 - i % 8)) & 1));
        }
    }
}

}  // namespace

int main() {
    checkCompressedGlyphs();
    using namespace platform::fonts;
    std::array<uint8_t, 480 * 800 / 8> pixels{};
    freeink::ui::DisplayTarget target(pixels.data(), 480, 800, 60);
    assert(!registerFonts(target));
    assert(loadFonts());
    assert(loadFonts("/missing/unused-after-startup"));
    assert(registerFonts(target));
    constexpr std::array<int16_t, 8> heights{28, 33, 48, 48, 55, 45, 76, 114};
    for (freeink::ui::FontId slot = 0; slot < heights.size(); ++slot) {
        assert(target.lineHeight(slot) == heights[slot]);
    }
    constexpr std::array<int16_t, 5> chineseSizes{22, 26, 30, 38, 43};
    for (freeink::ui::FontId slot = 0; slot < chineseSizes.size(); ++slot) {
        for (const char* text : {"李", "昕", "，", "！", "（", "）", "《", "》", "…"}) {
            assert(target.measureText(slot, text, {}).width == chineseSizes[slot]);
            pixels.fill(0xFF);
            target.text({0, 0, 480, 160}, text, {.font = slot});
            bool hasInk = false;
            for (const auto byte : pixels) hasInk |= byte != 0xFF;
            assert(hasInk);
        }
    }
    const auto clock = fontId(Font::NDot120);
    for (const char ch : "0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ") {
        if (!ch) continue;
        const char text[]{ch, '\0'};
        assert(target.measureText(clock, text, {}).width == (ch == ':' ? 24 : 72));
        pixels.fill(0xFF);
        target.text({0, 0, 480, 160}, text, {.font = clock});
        bool hasInk = false;
        for (const auto byte : pixels) hasInk |= byte != 0xFF;
        assert(hasInk);
    }
    for (int minute = 0; minute < 24 * 60; ++minute) {
        char time[6];
        std::snprintf(time, sizeof(time), "%02d:%02d", minute / 60, minute % 60);
        assert(target.measureText(clock, time, {}).width == 312);
    }
    for (freeink::ui::FontId slot = 0; slot < clock; ++slot) {
        for (char ch = '!'; ch <= '~'; ++ch) {
            const char text[]{ch, '\0'};
            assert(target.measureText(slot, text, {}).width > 0);
            pixels.fill(0xFF);
            target.text({0, 0, 480, 160}, text, {.font = slot});
            bool hasInk = false;
            for (const auto byte : pixels) hasInk |= byte != 0xFF;
            assert(hasInk);
        }
    }
    std::puts("Font registration, printable glyphs and fixed-width clock checks passed");
}
