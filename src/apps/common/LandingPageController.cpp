#include "LandingPageController.h"

#include <limits>
#include <utility>

#include "../../platform/runtime/Shell.h"

namespace apps::common {

LandingPageController::LandingPageController(std::string title, int initialCount)
    : _title(std::move(title)), _count(initialCount) {
}

void LandingPageController::onEnter(std::string_view) {
    _rendered.interactions.clear();
}

void LandingPageController::onLeave() {
    _rendered.interactions.clear();
}

void LandingPageController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    _view.render(canvas, bounds, {.title = _title.c_str(), .count = _count}, _rendered);
}

bool LandingPageController::onInput(const platform::runtime::InputEvent& event) {
    using Type = platform::runtime::InputEvent::Type;
    if (event.type == Type::Back) return platform::runtime::Shell::instance().goHome();
    if (event.type != Type::TouchRelease) return false;
    const auto action = _rendered.interactions.route({.touchReleased = true, .touchX = event.x, .touchY = event.y});
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
