#include "StatusBar.h"

#include <FreeInkUILayout.h>

#include <cstdio>

#include "../../../platform/fonts/Fonts.h"

namespace apps::shell::components {

void StatusBar::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props) const {
    using namespace freeink::ui;
    const bool dark = props.theme == platform::ui::Theme::Dark;
    canvas.fill(bounds, Paint::solid(dark ? Color::Black : Color::White));
    const Rect content = bounds.inset(Insets{.top = 0, .right = 12, .bottom = 0, .left = 12});
    Rect slots[3]{};
    // Empty center content measures zero; both side slots share the remaining width.
    layoutLinear(
        content, Axis::Row, 0, 3,
        [](uint8_t index) { return index == 1 ? LayoutLength::fixed(0) : LayoutLength::flexible(); },
        [&](uint8_t index, Rect rect) { slots[index] = rect; });
    char time[6];
    snprintf(time, sizeof(time), "%02u:%02u", static_cast<unsigned>(props.hour), static_cast<unsigned>(props.minute));
    canvas.text(
        slots[0], time,
        {.font = platform::fonts::fontId(platform::fonts::Font::RobotoS), .color = dark ? Color::White : Color::Black});
    views::BatteryIndicatorView{}.render(canvas, slots[2],
                                         {.percent = props.percent, .charging = props.charging, .theme = props.theme});
}

}  // namespace apps::shell::components
