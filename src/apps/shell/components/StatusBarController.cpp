#include "StatusBarController.h"

#include "../../../platform/runtime/Shell.h"

namespace apps::shell::components {

void StatusBarController::reset() {
    _minuteRevision = 0;
    _battery.reset();
}

void StatusBarController::setTheme(platform::ui::Theme theme) {
    _themeChanged = _themeChanged || _theme != theme;
    _theme = theme;
}

bool StatusBarController::update() {
    const auto revision = platform::runtime::Shell::instance().services().time().minuteRevision();
    const bool clockChanged = _minuteRevision != revision;
    _minuteRevision = revision;
    const bool changed = _battery.update() || clockChanged || _themeChanged;
    _themeChanged = false;
    return changed;
}

void StatusBarController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    const auto& time = platform::runtime::Shell::instance().services().time().displayTime();
    const auto battery = _battery.props(_theme);
    _view.render(canvas, bounds,
                 {.hour = time.hour,
                  .minute = time.minute,
                  .percent = battery.percent,
                  .charging = battery.charging,
                  .theme = battery.theme});
}

}  // namespace apps::shell::components
