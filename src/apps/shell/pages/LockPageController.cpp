#include "LockPageController.h"

#include <Arduino.h>

#include "../../../platform/hal/Hardware.h"
#include "../../../platform/runtime/Shell.h"

namespace apps::shell::pages {

void LockPageController::onEnter(std::string_view) {
    _minuteRevision = 0;
    _powerSampled = false;
    _powerKnown = false;
    _charging = false;
    _batteryKnown = false;
    _showUnlockHint = false;
    _hintChanged = false;
    (void)update();
}

bool LockPageController::update() {
    if (_showUnlockHint && static_cast<uint32_t>(millis()) - _hintShownAt >= 5000U) {
        _showUnlockHint = false;
        _hintChanged = true;
    }
    const bool hintChanged = _hintChanged;
    _hintChanged = false;
    const auto revision = platform::runtime::Shell::instance().services().time().minuteRevision();
    const bool clockChanged = _minuteRevision != revision;
    _minuteRevision = revision;
    return _samplePower(clockChanged) || clockChanged || hintChanged;
}

bool LockPageController::onInput(const platform::runtime::InputEvent& event) {
    using Type = platform::runtime::InputEvent::Type;
    if (event.type != Type::PowerPress && event.type != Type::TouchPress && event.type != Type::TouchRelease &&
        event.type != Type::Swipe)
        return false;
    _hintChanged = _hintChanged || !_showUnlockHint;
    _showUnlockHint = true;
    _hintShownAt = static_cast<uint32_t>(millis());
    return true;
}

bool LockPageController::_samplePower(bool refreshPercent) {
    const auto now = static_cast<uint32_t>(millis());
    if (_powerSampled && !refreshPercent && now - _powerSampledAt < 1000U) return false;
    _powerSampled = true;
    _powerSampledAt = now;
    bool charging = false;
    bool changed = false;
    if (platform::hal::readCharging(charging)) {
        changed = charging != _charging;
        if (_powerKnown && changed)
            platform::runtime::Shell::instance().services().power().notifyPowerConnectionChanged();
        _powerKnown = true;
        _charging = charging;
    }
    if (_charging && (refreshPercent || changed || !_batteryKnown)) {
        uint8_t percent = 0;
        if (platform::hal::readBatteryPercent(percent)) {
            changed = changed || !_batteryKnown || percent != _percent;
            _percent = percent;
            _batteryKnown = true;
        }
    }
    if (!_charging) _batteryKnown = false;
    return changed;
}

void LockPageController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    const auto& time = platform::runtime::Shell::instance().services().time().displayTime();
    _view.render(canvas, bounds,
                 {.hour = time.hour,
                  .minute = time.minute,
                  .month = time.month,
                  .day = time.day,
                  .weekday = time.weekday,
                  .charging = _charging,
                  .batteryKnown = _batteryKnown,
                  .percent = _percent,
                  .showUnlockHint = _showUnlockHint});
}

}  // namespace apps::shell::pages
