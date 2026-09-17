#include "HomePageController.h"

#include "../../../platform/runtime/Shell.h"

namespace apps::shell::pages {

void HomePageController::onEnter(std::string_view) {
    _rendered.interactions.clear();
}

void HomePageController::onLeave() {
    _rendered.interactions.clear();
}

void HomePageController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    _view.render(canvas, bounds, {}, _rendered);
}

bool HomePageController::onInput(const platform::runtime::InputEvent& event) {
    if (event.type != platform::runtime::InputEvent::Type::TouchRelease) return false;
    const auto action = _rendered.interactions.route({.touchReleased = true, .touchX = event.x, .touchY = event.y});
    auto& shell = platform::runtime::Shell::instance();
    if (action.action == 1) return shell.open("app://calendar/");
    if (action.action == 2) return shell.open("app://test/");
    if (action.action == 3) return shell.open("app://typography/");
    return false;
}

}  // namespace apps::shell::pages
