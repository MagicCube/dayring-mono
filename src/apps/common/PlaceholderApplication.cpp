#include "PlaceholderApplication.h"

#include <components/controls/button.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "../../platform/runtime/Shell.h"

namespace apps::common {
PlaceholderApplication::PlaceholderApplication(std::string title) : _page(std::move(title)) {
}

void PlaceholderApplication::onCreate() {
    (void)router().registerPage("/", _page);
}

void PlaceholderApplication::onEnter(const platform::runtime::Intent& intent) {
    if (intent.reason != platform::runtime::Intent::Reason::Open) return;
    if (!navigation().replace(intent.url.location.c_str())) (void)navigation().replace("/");
}

void PlaceholderApplication::onLeave() {
}

void PlaceholderApplication::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    if (auto* page = navigation().currentPage()) {
        page->render(canvas, bounds);
    }
}

bool PlaceholderApplication::onInput(const platform::runtime::InputEvent& event) {
    auto* page = navigation().currentPage();
    if (!page || !page->onInput(event)) return false;
    requestRender();
    return true;
}

PlaceholderApplication::LandingPage::LandingPage(std::string title) : _title(std::move(title)) {
}

void PlaceholderApplication::LandingPage::onEnter(std::string_view) {
    _interactions.clear();
}

void PlaceholderApplication::LandingPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    canvas.fill(bounds, freeink::ui::Paint::solid(freeink::ui::Color::White));
    const freeink::ui::DeviceContext device{.width = bounds.right(), .height = bounds.bottom()};
    const freeink::ui::InputSnapshot input{};
    freeink::ui::Frame<3> frame(canvas, device, input, _interactions);
    const auto width = static_cast<int16_t>(std::clamp<int>(bounds.width - 48, 0, 320));
    const auto x = static_cast<int16_t>(bounds.x + (bounds.width - width) / 2);
    const auto y = static_cast<int16_t>(bounds.y + bounds.height / 2);
    canvas.text({x, static_cast<int16_t>(y - 88), width, 64}, _title.c_str(),
                {.align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::Black});
    freeink::ui::button(frame, {x, y, width, 64},
                        {.label = "Back to Home", .action = 1, .inputMask = freeink::ui::InputTouch, .radius = 12});
    const auto value = std::to_string(_count);
    canvas.text({x, static_cast<int16_t>(y + 88), width, 64}, value.c_str(),
                {.align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::Black});
    const auto buttonWidth = static_cast<int16_t>(std::max<int>(0, (width - 24) / 2));
    freeink::ui::button(frame, {x, static_cast<int16_t>(y + 176), buttonWidth, 64},
                        {.label = "-", .action = 2, .inputMask = freeink::ui::InputTouch, .radius = 12});
    freeink::ui::button(frame,
                        {static_cast<int16_t>(x + width - buttonWidth), static_cast<int16_t>(y + 176), buttonWidth, 64},
                        {.label = "+", .action = 3, .inputMask = freeink::ui::InputTouch, .radius = 12});
}

bool PlaceholderApplication::LandingPage::onInput(const platform::runtime::InputEvent& event) {
    using Type = platform::runtime::InputEvent::Type;
    if (event.type == Type::Back) return platform::runtime::Shell::instance().goHome();
    if (event.type != Type::TouchRelease) return false;
    const auto action = _interactions.route({.touchReleased = true, .touchX = event.x, .touchY = event.y});
    if (action.action == 1) return platform::runtime::Shell::instance().goHome();
    if (action.action == 2 && _count > std::numeric_limits<int>::min()) {
        --_count;
    } else if (action.action == 3 && _count < std::numeric_limits<int>::max()) {
        ++_count;
    } else {
        return false;
    }
    return true;
}
}  // namespace apps::common
