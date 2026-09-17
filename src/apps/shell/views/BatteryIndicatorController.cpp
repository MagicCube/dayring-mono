#include "BatteryIndicatorController.h"

#include <Arduino.h>

#include "../../../platform/hal/Hardware.h"

namespace apps::shell::views {

void BatteryIndicatorController::reset() {
    _sampled = false;
}

bool BatteryIndicatorController::update() {
    bool changed = _themeChanged;
    _themeChanged = false;
    const uint32_t now = millis();
    if (!_sampled || now - _chargingSampledAt >= 1000U) {
        bool charging = _charging;
        if (platform::hal::readCharging(charging) && charging != _charging) {
            _charging = charging;
            changed = true;
        }
        _chargingSampledAt = now;
    }
    if (!_sampled || now - _batterySampledAt >= 180000U) {
        uint8_t percent = _percent;
        if (platform::hal::readBatteryPercent(percent)) {
            changed = changed || !_batteryKnown || percent != _percent;
            _percent = percent;
            _batteryKnown = true;
        }
        _batterySampledAt = now;
    }
    _sampled = true;
    return changed;
}

BatteryIndicatorProps BatteryIndicatorController::props(platform::ui::Theme theme) const {
    return {.percent = _percent, .charging = _charging, .theme = theme};
}

void BatteryIndicatorController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    _view.render(canvas, bounds, props(_theme));
}

void BatteryIndicatorController::setTheme(platform::ui::Theme theme) {
    _themeChanged = _themeChanged || _theme != theme;
    _theme = theme;
}

}  // namespace apps::shell::views
