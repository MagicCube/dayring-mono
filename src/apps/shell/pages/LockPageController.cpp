#include "LockPageController.h"

#include "../../../platform/hal/Hardware.h"

namespace apps::shell::pages {

void LockPageController::onEnter(std::string_view) {
    _clock.reset();
    (void)update();
}

bool LockPageController::update() {
    if (!_clock.update()) return false;
    bool charging = false;
    _charging = platform::hal::readCharging(charging) && charging;
    _batteryKnown = _charging && platform::hal::readBatteryPercent(_percent);
    return true;
}

void LockPageController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    const auto& time = _clock.time();
    _view.render(canvas, bounds,
                 {.hour = time.hour,
                  .minute = time.minute,
                  .month = time.month,
                  .day = time.day,
                  .weekday = time.weekday,
                  .charging = _charging,
                  .batteryKnown = _batteryKnown,
                  .percent = _percent});
}

}  // namespace apps::shell::pages
