#include "BatteryIndicatorView.h"

#include <algorithm>
#include <array>

namespace apps::shell::views {
namespace {

using namespace freeink::ui;
// Pixel-authored 13 x 24 silhouette keeps the diagonal crisp at status-bar scale.
constexpr std::array<uint16_t, 24> bolt{0x0030, 0x0070, 0x00E0, 0x01E0, 0x03C0, 0x0780, 0x0F80, 0x0F00,
                                        0x1E00, 0x1FFC, 0x1FFC, 0x0FF8, 0x0078, 0x00F0, 0x00F0, 0x01E0,
                                        0x01C0, 0x0380, 0x0380, 0x0700, 0x0600, 0x0C00, 0x0800, 0x0800};

void drawBolt(DrawTarget& canvas, const Rect& body) {
    const int16_t x = body.x + (body.width - 13) / 2;
    const int16_t y = body.y + (body.height - 24) / 2;
    for (int pass = 0; pass < 2; ++pass) {
        for (int row = 0; row < 24; ++row) {
            for (int col = 0; col < 13; ++col) {
                if ((bolt[row] & (1 << (12 - col))) == 0) continue;
                const int padding = pass == 0 ? 1 : 0;
                canvas.fill({static_cast<int16_t>(x + col - padding), static_cast<int16_t>(y + row - padding),
                             static_cast<int16_t>(1 + 2 * padding), static_cast<int16_t>(1 + 2 * padding)},
                            Paint::solid(pass == 0 ? Color::Black : Color::White));
            }
        }
    }
}

}  // namespace

void BatteryIndicatorView::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds,
                                  const Props& props) const {
    using namespace freeink::ui;
    if (bounds.width < 40 || bounds.height < 26) return;
    const Rect body{static_cast<int16_t>(bounds.right() - 40),
                    static_cast<int16_t>(bounds.y + (bounds.height - 20) / 2), 35, 20};
    const auto foreground = Paint::solid(props.theme == platform::ui::Theme::Dark ? Color::White : Color::Black);
    const auto track = Paint::solid(Color::LightGray);
    const int fillWidth = (35 * std::min<int>(props.percent, 100) + 50) / 100;
    // Shared silhouette for both colors: no inset, outline, or rounded fill boundary.
    constexpr std::array<int16_t, 5> insets{3, 2, 1, 0, 0};
    for (int16_t row = 0; row < 20; ++row) {
        const int edge = std::min<int>(row, 19 - row);
        const int16_t inset = edge < 5 ? insets[edge] : 0;
        const int16_t end = 35 - inset;
        // Gray pixels are transparent in the BW adapter; provide their white substrate.
        canvas.fill({static_cast<int16_t>(body.x + inset), static_cast<int16_t>(body.y + row),
                     static_cast<int16_t>(end - inset), 1},
                    Paint::solid(Color::White));
        const int16_t split = std::clamp<int>(fillWidth, inset, end);
        if (split > inset)
            canvas.fill({static_cast<int16_t>(body.x + inset), static_cast<int16_t>(body.y + row),
                         static_cast<int16_t>(split - inset), 1},
                        foreground);
        if (end > split)
            canvas.fill({static_cast<int16_t>(body.x + split), static_cast<int16_t>(body.y + row),
                         static_cast<int16_t>(end - split), 1},
                        track);
    }
    canvas.fill({static_cast<int16_t>(body.right() + 2), static_cast<int16_t>(body.y + 6), 3, 8},
                Paint::solid(Color::White), 1);
    canvas.fill({static_cast<int16_t>(body.right() + 2), static_cast<int16_t>(body.y + 6), 3, 8},
                props.percent >= 100 ? foreground : track, 1);
    if (props.charging) drawBolt(canvas, body);
}

}  // namespace apps::shell::views
