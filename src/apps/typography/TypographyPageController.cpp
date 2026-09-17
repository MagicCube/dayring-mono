#include "TypographyPageController.h"

#include <cstdlib>

#include "../../platform/runtime/AppURL.h"
#include "../../platform/runtime/Shell.h"

namespace apps::typography {

void TypographyPageController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    _view.render(canvas, bounds, {.isDisplayArticle = _isDisplayArticle}, _rendered);
    _hasRendered = true;
}

bool TypographyPageController::acceptsLocation(std::string_view location) const {
    const auto query = platform::runtime::LocationQuery::parse(location);
    if (!query) return false;
    const auto article = query->value("article");
    return !article || *article == "reading" || *article == "display";
}

void TypographyPageController::onEnter(std::string_view location) {
    const auto query = platform::runtime::LocationQuery::parse(location);
    _isDisplayArticle = query && query->value("article") == "display";
    _hasRendered = false;
}

bool TypographyPageController::onInput(const platform::runtime::InputEvent& event) {
    using Type = platform::runtime::InputEvent::Type;
    if (event.type == Type::Back) return platform::runtime::Shell::instance().goHome();
    if (!_hasRendered) return false;
    bool next;
    if (event.type == Type::TouchRelease && _rendered.bounds.contains(event.x, event.y)) {
        if (_rendered.previous.contains(event.x, event.y)) {
            next = false;
        } else if (_rendered.next.contains(event.x, event.y)) {
            next = true;
        } else {
            return false;
        }
    } else if (event.type == Type::Swipe && _rendered.bounds.contains(event.startX, event.startY)) {
        const int dx = event.x - event.startX;
        const int dy = event.y - event.startY;
        if (std::abs(dx) < 40 || std::abs(dx) <= std::abs(dy)) return false;
        next = dx < 0;
    } else {
        return false;
    }
    if (next == _isDisplayArticle) return false;
    _isDisplayArticle = next;
    _hasRendered = false;
    return true;
}

}  // namespace apps::typography
