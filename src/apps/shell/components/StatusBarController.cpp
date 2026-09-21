#include "StatusBarController.h"

#include <Arduino.h>

#include "../../../platform/runtime/Shell.h"

namespace apps::shell::components {

void StatusBarController::reset() {
    _minuteRevision = 0;
    _battery.reset();
    _bluetooth = BluetoothState::Disconnected;
    _bluetoothVisible = true;
}

void StatusBarController::setTheme(platform::ui::Theme theme) {
    _themeChanged = _themeChanged || _theme != theme;
    _theme = theme;
}

bool StatusBarController::update() {
    const auto revision = platform::runtime::Shell::instance().services().time().minuteRevision();
    const bool clockChanged = _minuteRevision != revision;
    _minuteRevision = revision;
    const bool bluetoothChanged = _updateBluetooth();
    const bool changed = _battery.update() || bluetoothChanged || clockChanged || _themeChanged;
    _themeChanged = false;
    return changed;
}

bool StatusBarController::_updateBluetooth() {
    using State = platform::ble::BLEService::State;
    const auto state = platform::runtime::Shell::instance().services().ble().state();
    const auto next = state == State::Secured     ? BluetoothState::Connected
                      : state == State::Connected ? BluetoothState::Connecting
                                                  : BluetoothState::Disconnected;
    const uint32_t now = millis();
    if (next != _bluetooth) {
        _bluetooth = next;
        _bluetoothVisible = true;
        _blinkAt = now;
        return true;
    }
    if (_bluetooth == BluetoothState::Connecting && now - _blinkAt >= 1000U) {
        _bluetoothVisible = !_bluetoothVisible;
        _blinkAt = now;
        return true;
    }
    return false;
}

void StatusBarController::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    const auto& time = platform::runtime::Shell::instance().services().time().displayTime();
    const auto battery = _battery.props(_theme);
    _view.render(canvas, bounds,
                 {.hour = time.hour,
                  .minute = time.minute,
                  .percent = battery.percent,
                  .charging = battery.charging,
                  .bluetooth = _bluetooth,
                  .bluetoothVisible = _bluetoothVisible,
                  .theme = battery.theme});
}

}  // namespace apps::shell::components
