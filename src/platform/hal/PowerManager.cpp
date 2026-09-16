#include "PowerManager.h"

#include <Arduino.h>

#include <algorithm>

namespace platform::hal {
void PowerManager::begin() {
    _locked = false;
    _hasLocked = false;
    _beginFrontlight();
}

void PowerManager::update(bool inputActive) {
    if (inputActive) notifyActivity();
    _updateFrontlight();
}

void PowerManager::notifyActivity() {
    if (_locked) return;
    _activateFrontlight();
}

bool PowerManager::isIdleLockDue() const {
    return !_locked && static_cast<uint32_t>(static_cast<uint32_t>(millis()) - _frontlightLastActivityMs) >=
                           _frontlightOffTimeoutMs;
}

void PowerManager::setLocked(bool locked, LockReason reason) {
    if (_locked == locked) return;
    _locked = locked;
    if (!_locked) {
        _activateFrontlight();
    } else if (!_hasLocked && reason == LockReason::Manual) {
        // Keep the first lock page visible for ten seconds after boot.
        _hasLocked = true;
        _activateFrontlight();
    } else {
        _hasLocked = true;
        _setFrontlightBrightness(0);
    }
}

void PowerManager::_beginFrontlight() {
    _frontlightDriver.begin();
    _frontlightDriver.off();
    _frontlightBrightness = 0;
    // Light immediately so board bring-up has visible feedback before the first frame.
    _activateFrontlight();
}

void PowerManager::_updateFrontlight() {
    // Sample after activation so a clock tick cannot underflow the idle duration.
    const auto now = static_cast<uint32_t>(millis());
    const auto idleMs = static_cast<uint32_t>(now - _frontlightLastActivityMs);
    if (_locked) {
        if (idleMs >= _frontlightLockTimeoutMs) _setFrontlightBrightness(0);
        return;
    }
    if (idleMs >= _frontlightOffTimeoutMs) {
        _setFrontlightBrightness(0);
    } else if (idleMs >= _frontlightDimTimeoutMs) {
        _setFrontlightBrightness(_frontlightDimmedBrightness);
    }
}

void PowerManager::_activateFrontlight() {
    _frontlightLastActivityMs = static_cast<uint32_t>(millis());
    _setFrontlightBrightness(_frontlightActiveBrightness);
}

void PowerManager::_setFrontlightBrightness(uint8_t percent) {
    percent = std::min<uint8_t>(percent, 100);
    if (_frontlightBrightness == percent) return;
    _frontlightDriver.setBrightness(percent);
    _frontlightBrightness = percent;
}
}  // namespace platform::hal
