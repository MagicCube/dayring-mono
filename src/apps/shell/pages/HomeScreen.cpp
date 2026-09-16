#include "HomeScreen.h"

#include <FreeInkUILayout.h>
#include <components/controls/button.h>

#include <algorithm>

#include "../../../platform/runtime/Shell.h"

namespace apps::shell::pages {
void HomeScreen::onEnter(std::string_view) {
    _interactions.clear();
}

void HomeScreen::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    canvas.fill(bounds, freeink::ui::Paint::solid(freeink::ui::Color::White));
    const freeink::ui::DeviceContext device{.width = bounds.right(), .height = bounds.bottom()};
    const freeink::ui::InputSnapshot input{};
    freeink::ui::Frame<3> frame(canvas, device, input, _interactions);
    auto styles = freeink::ui::defaultButtonStyles();
    styles.normal.border = freeink::ui::Paint::solid(freeink::ui::Color::Black);
    styles.normal.borderWidth = 2;

    constexpr int16_t padding = 24;
    constexpr int16_t gap = 24;
    const auto width = static_cast<int16_t>(std::clamp<int>(bounds.width - padding * 2, 0, 320));
    constexpr int16_t height = 64;
    const auto x = static_cast<int16_t>(bounds.x + (bounds.width - width) / 2);
    const auto totalHeight = static_cast<int16_t>(height * 3 + gap * 2);
    const auto y = static_cast<int16_t>(bounds.y + (bounds.height - totalHeight) / 2);
    const freeink::ui::Rect menu{x, y, width, totalHeight};
    constexpr const char* labels[]{"Calendar", "Test", "Typography"};
    freeink::ui::layoutLinear(
        menu, freeink::ui::Axis::Column, gap, 3, [](uint8_t) { return freeink::ui::LayoutLength::fixed(height); },
        [&](uint8_t index, freeink::ui::Rect slot) {
            freeink::ui::button(frame, slot,
                                {.label = labels[index],
                                 .action = static_cast<freeink::ui::ActionId>(index + 1),
                                 .inputMask = freeink::ui::InputTouch,
                                 .styles = styles,
                                 .radius = 12});
        });
}

bool HomeScreen::onInput(const platform::runtime::InputEvent& event) {
    if (event.type != platform::runtime::InputEvent::Type::TouchRelease) return false;
    const auto action = _interactions.route({.touchReleased = true, .touchX = event.x, .touchY = event.y});
    auto& shell = platform::runtime::Shell::instance();
    if (action.action == 1) return shell.open("app://calendar/");
    if (action.action == 2) return shell.open("app://test/");
    if (action.action == 3) return shell.open("app://typography/");
    return false;
}
}  // namespace apps::shell::pages
