#include "StatusBarController.h"

namespace apps::shell::components {

void StatusBarController::reset() {
    _clock.reset();
    _battery.reset();
}

void StatusBarController::setTheme(platform::ui::Theme theme) {
    _themeChanged = _themeChanged || _theme != theme;
    _theme = theme;
}

bool StatusBarController::update() {
    const bool clockChanged = _clock.update();
    const bool changed = _battery.update() || clockChanged || _themeChanged;
    _themeChanged = false;
    return changed;
}

void StatusBarController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    const auto battery = _battery.props(_theme);
    _view.render(canvas, bounds,
                 {.hour = _clock.time().hour,
                  .minute = _clock.time().minute,
                  .percent = battery.percent,
                  .charging = battery.charging,
                  .theme = battery.theme});
}

}  // namespace apps::shell::components
