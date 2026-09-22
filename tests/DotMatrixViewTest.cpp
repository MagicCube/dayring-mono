#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <string_view>

#include "apps/shell/pages/LockWeather.h"
#include "platform/ui/DotMatrixView.h"

namespace {

void checkGeometry(uint8_t size, uint8_t diameter, freeink::ui::Color color) {
    using namespace platform::ui;
    std::array<uint8_t, 160 * 150 / 8> frame;
    const bool black = color == freeink::ui::Color::Black;
    frame.fill(black ? 255 : 0);
    freeink::ui::DisplayTarget canvas(frame.data(), 160, 150, 20, freeink::ui::Orientation::LandscapeCounterClockwise);
    DotMatrix full;
    full.size = size;
    full.rows.fill((1u << size) - 1);
    DotMatrixView{}.render(canvas, {0, 0, 160, 150}, {.matrix = full, .color = color, .dotDiameter = diameter});
    const int extent = size * diameter;
    const int left = (160 - extent) / 2, top = (150 - extent) / 2;
    for (int y = 0; y < 150; ++y) {
        for (int x = 0; x < 160; ++x) {
            bool inside = false;
            if (x >= left && x < left + extent && y >= top && y < top + extent) {
                const int dx = 2 * ((x - left) % diameter) + 1 - diameter;
                const int dy = 2 * ((y - top) % diameter) + 1 - diameter;
                inside = dx * dx + dy * dy <= diameter * diameter;
            }
            const bool isBlack = (frame[y * 20 + x / 8] & (0x80u >> (x % 8))) == 0;
            assert(isBlack == (inside ? black : !black));
        }
    }
    frame.fill(255);
    DotMatrixView{}.render(canvas, {0, 0, static_cast<int16_t>(extent - 1), 150},
                           {.matrix = full, .dotDiameter = diameter});
    for (const auto byte : frame) assert(byte == 255);
    DotMatrixView{}.render(canvas, {0, 0, 160, 150}, {});
    DotMatrixView{}.render(canvas, {0, 0, 160, 150}, {.matrix = full, .dotDiameter = 0});
    full.size = 17;
    DotMatrixView{}.render(canvas, {0, 0, 160, 150}, {.matrix = full});
    full.size = 0;
    DotMatrixView{}.render(canvas, {0, 0, 160, 150}, {.matrix = full});
    for (const auto byte : frame) assert(byte == 255);
}

void checkWeatherCoverage() {
    using namespace apps::shell;
    constexpr std::array codes{113, 116, 119, 122, 143, 176, 179, 182, 185, 200, 227, 230, 248, 260, 263, 266,
                               281, 284, 293, 296, 299, 302, 305, 308, 311, 314, 317, 320, 323, 326, 329, 332,
                               335, 338, 350, 353, 356, 359, 362, 365, 368, 371, 374, 377, 386, 389, 392, 395};
    const std::array matrices{icons::Clear, icons::PartlyCloudy, icons::Cloudy,
                              icons::Rain,  icons::Thunderstorm, icons::Snow};
    std::array<int, 6> counts{};
    for (const auto code : codes) {
        const auto mapped = pages::weatherIcon(code);
        assert(mapped.size == 11);
        int matches = 0;
        for (size_t i = 0; i < matrices.size(); ++i) {
            if (mapped.rows != matrices[i].rows) continue;
            ++counts[i];
            ++matches;
        }
        assert(matches == 1);
        for (const auto row : mapped.rows) assert((row & ~2047) == 0);
    }
    assert((counts == std::array{1, 1, 5, 12, 5, 24}));
    assert(pages::weatherIcon(296).rows == pages::weatherIcon(308).rows);
    assert(pages::weatherIcon(395).rows == icons::Thunderstorm.rows);
    assert(pages::weatherIcon(311).rows == icons::Snow.rows);
    assert(pages::weatherIcon(182).rows == icons::Snow.rows);
    assert(pages::weatherIcon(248).rows == icons::Cloudy.rows);
    assert(std::string_view(pages::weatherCondition(296)) == "Light rain");
    assert(std::string_view(pages::weatherCondition(308)) == "Heavy rain");
    assert(std::string_view(pages::weatherCondition(311)) == "Freezing rain");
    assert(std::string_view(pages::weatherCondition(182)) == "Patchy sleet");
    assert(std::string_view(pages::weatherCondition(248)) == "Fog");
    assert(std::string_view(pages::weatherCondition(122)) == "Overcast");
    assert(pages::weatherIcon(0).rows == platform::ui::DotMatrix{}.rows);
    assert(pages::weatherIcon(999).rows == platform::ui::DotMatrix{}.rows);
}

}  // namespace

int main() {
    constexpr platform::ui::DotMatrix corners{.size = 11, .rows = {1025, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1025}};
    static_assert(corners.isSet(0, 0) && corners.isSet(10, 10));
    static_assert(!corners.isSet(11, 0) && !corners.isSet(0, 11) && !corners.isSet(1, 0));
    for (const uint8_t size : {7, 9, 11}) {
        for (const uint8_t diameter : {2, 5, 8, 9, 12}) {
            checkGeometry(size, diameter, freeink::ui::Color::Black);
            checkGeometry(size, diameter, freeink::ui::Color::White);
        }
    }
    checkWeatherCoverage();
    std::puts("7/9/11 grids, circle geometry, scale, colors, bounds and all 48 WWO codes passed");
}
