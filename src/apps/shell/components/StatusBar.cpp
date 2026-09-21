#include "StatusBar.h"

#include <FreeInkUILayout.h>

#include <cstdio>

#include "../../../platform/fonts/Fonts.h"

namespace apps::shell::components {
namespace {

void drawBluetooth(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const StatusBarProps& props) {
    using namespace freeink::ui;
    if (!props.bluetoothVisible) return;
    const auto foreground = Paint::solid(props.theme == platform::ui::Theme::Dark ? Color::White : Color::Black);
    const int16_t x = bounds.x;
    const int16_t y = bounds.y + (bounds.height - 28) / 2;
    const auto point = [=](int dx, int dy) {
        return Point{static_cast<int16_t>(x + dx), static_cast<int16_t>(y + dy)};
    };
    canvas.line(point(9, 5), point(9, 22), 2, foreground);
    canvas.line(point(9, 5), point(15, 10), 2, foreground);
    canvas.line(point(15, 10), point(4, 19), 2, foreground);
    canvas.line(point(4, 8), point(15, 17), 2, foreground);
    canvas.line(point(15, 17), point(9, 22), 2, foreground);
    if (props.bluetooth == BluetoothState::Disconnected) {
        canvas.fill({static_cast<int16_t>(x + 19), static_cast<int16_t>(y + 7), 3, 10}, foreground);
        canvas.fill({static_cast<int16_t>(x + 19), static_cast<int16_t>(y + 20), 3, 3}, foreground);
    }
}

}  // namespace

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
    constexpr int16_t bluetoothWidth = 22;
    constexpr int16_t gap = 6;
    constexpr int16_t batteryWidth = 40;
    drawBluetooth(canvas,
                  {static_cast<int16_t>(content.right() - batteryWidth - gap - bluetoothWidth), content.y,
                   bluetoothWidth, content.height},
                  props);
    views::BatteryIndicatorView{}.render(canvas, slots[2],
                                         {.percent = props.percent, .charging = props.charging, .theme = props.theme});
}

}  // namespace apps::shell::components
