#include "ApplicationContainer.h"

#include <algorithm>

#include "../runtime/ApplicationManager.h"
#include "PageController.h"

namespace platform::ui {

void ApplicationContainer::showStatusBar() {
    if (auto* page = _applications.currentPageController()) page->setFullscreen(false);
    _synchronizeVisibility();
}

void ApplicationContainer::hideStatusBar() {
    if (auto* page = _applications.currentPageController()) page->setFullscreen(true);
    _synchronizeVisibility();
}

bool ApplicationContainer::isStatusBarVisible() const {
    const auto* page = _applications.currentPageController();
    return page && !page->isFullscreen();
}

void ApplicationContainer::_synchronizeVisibility() {
    const bool visible = isStatusBarVisible();
    if (visible == _statusBarVisible) return;
    _statusBarVisible = visible;
    _dirty = true;
    if (visible) _statusBar.reset();
}

void ApplicationContainer::update() {
    _synchronizeVisibility();
    if (_statusBarVisible) _dirty = _statusBar.update() || _dirty;
}

bool ApplicationContainer::needsRender() const {
    return _dirty || _applications.needsRender() || isStatusBarVisible() != _statusBarVisible;
}

void ApplicationContainer::render(Canvas& canvas, const Rect& bounds) {
    update();
    _bounds = bounds;
    const int16_t height = _statusBarVisible ? std::min(bounds.height, apps::shell::components::StatusBar::kHeight) : 0;
    _contentBounds = {bounds.x, static_cast<int16_t>(bounds.y + height), bounds.width,
                      static_cast<int16_t>(bounds.height - height)};
    canvas.fill(bounds, freeink::ui::Paint::solid(freeink::ui::Color::White));
    (void)_applications.render(canvas, _contentBounds, true);
    if (_statusBarVisible) _statusBar.render(canvas, {bounds.x, bounds.y, bounds.width, height});
    _renderedPage = _applications.currentPageController();
    _renderedStatusBarVisible = _statusBarVisible;
    _dirty = false;
}

bool ApplicationContainer::onInput(const runtime::InputEvent& event, int16_t minimumHomeSwipeDistance) {
    using Type = runtime::InputEvent::Type;
    if (event.type == Type::Swipe) {
        if (!_renderedPage || _applications.currentPageController() != _renderedPage ||
            isStatusBarVisible() != _renderedStatusBarVisible)
            return false;
        constexpr int kBottomBandHeight = 72;
        if (_homeGesture && _bounds.height > 0 && event.startY - event.y >= minimumHomeSwipeDistance &&
            _bounds.contains(event.startX, event.startY) &&
            freeink::ui::edgeSwipe(
                freeink::ui::ScreenEdge::Bottom, event.startX - _bounds.x, event.startY - _bounds.y,
                event.x - _bounds.x, event.y - _bounds.y, _bounds.width, _bounds.height,
                static_cast<float>(std::min<int>(kBottomBandHeight, _bounds.height)) / _bounds.height)) {
            _homeGesture();
            return true;
        }
    }
    if (event.type == Type::TouchPress || event.type == Type::TouchRelease) {
        // Hit regions still describe the last rendered layout until the pending frame commits.
        if (_applications.currentPageController() != _renderedPage || isStatusBarVisible() != _renderedStatusBarVisible)
            return false;
        if (!_contentBounds.contains(event.x, event.y)) return false;
    }
    return _applications.onInput(event);
}

}  // namespace platform::ui
