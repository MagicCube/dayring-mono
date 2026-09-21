#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cassert>
#include <cstdio>

#include "platform/fonts/Fonts.h"

int main() {
    using namespace platform::fonts;
    std::array<uint8_t, 480 * 800 / 8> pixels{};
    freeink::ui::DisplayTarget target(pixels.data(), 480, 800, 60);
    registerFonts(target);
    constexpr std::array<int16_t, 8> heights{27, 33, 38, 48, 55, 45, 76, 114};
    for (freeink::ui::FontId slot = 0; slot < heights.size(); ++slot) {
        assert(target.lineHeight(slot) == heights[slot]);
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
