#include "PowerService.h"

#include <Arduino.h>

#include <algorithm>

#include "../Hardware.h"

namespace platform::power {

PowerService::PowerService(frontlight::FrontlightService& frontlight) : _frontlight(frontlight) {
}

bool PowerService::start() {
    if (_running) return true;
    if (!_frontlight.isRunning()) return false;
    _running = true;
    _locked = false;
    _hasLocked = false;
    _lockInteractionLightActive = false;
    _activateFrontlight();
    return true;
}

void PowerService::stop() {
    _running = false;
}

bool PowerService::isRunning() const {
    return _running;
}

void PowerService::update(uint32_t) {
    if (!_running) return;
    if (!hal::powerButtonPressed() && hal::hasInputActivity()) notifyActivity();
    // Activity can sample a newer clock tick; sample after it to avoid underflow.
    _updateFrontlight();
}

void PowerService::notifyActivity() {
    if (!_running || _locked) return;
    _activateFrontlight();
}

void PowerService::notifyPowerConnectionChanged() {
    if (!_running || !_locked) return;
    const uint32_t elapsed = static_cast<uint32_t>(millis()) - _frontlightLastActivityMs;
    const uint32_t remaining =
        _lockInteractionLightActive && elapsed < _lockedLightDurationMs ? _lockedLightDurationMs - elapsed : 0;
    _lockedLightDurationMs = std::max<uint32_t>(5000U, remaining);
    _activateFrontlight();
}

void PowerService::notifyLockInteraction() {
    if (!_running || !_locked) return;
    _lockedLightDurationMs = lockInteractionDurationMs;
    _lockInteractionLightActive = true;
    _activateFrontlight();
}

void PowerService::toggleLockedLight() {
    if (!_running || !_locked) return;
    _lockInteractionLightActive = false;
    if (_frontlight.isOn()) {
        _frontlight.turnOff();
    } else {
        _lockedLightDurationMs = lockInteractionDurationMs;
        _activateFrontlight();
    }
}

bool PowerService::isIdleLockDue() const {
    return _running && !_locked &&
           static_cast<uint32_t>(static_cast<uint32_t>(millis()) - _frontlightLastActivityMs) >=
               _frontlightOffTimeoutMs;
}

void PowerService::setLocked(bool locked, LockReason reason) {
    if (!_running || _locked == locked) return;
    _locked = locked;
    _lockInteractionLightActive = false;
    _lockedLightDurationMs = _frontlightLockTimeoutMs;
    if (!_locked) {
        _activateFrontlight();
    } else if (!_hasLocked && reason == LockReason::Manual) {
        // Keep the first lock page visible for ten seconds after boot.
        _hasLocked = true;
        _activateFrontlight();
    } else {
        _hasLocked = true;
        _frontlight.setBrightness(0);
    }
}

void PowerService::_updateFrontlight() {
    // Sample after activation so a clock tick cannot underflow the idle duration.
    const auto now = static_cast<uint32_t>(millis());
    const auto idleMs = static_cast<uint32_t>(now - _frontlightLastActivityMs);
    if (_locked) {
        if (idleMs >= _lockedLightDurationMs) {
            _frontlight.setBrightness(0);
            _lockInteractionLightActive = false;
        }
        return;
    }
    if (idleMs >= _frontlightOffTimeoutMs) {
        _frontlight.setBrightness(0);
    } else if (idleMs >= _frontlightDimTimeoutMs) {
        _frontlight.setBrightness(_frontlightDimmedBrightness);
    }
}

void PowerService::_activateFrontlight() {
    _frontlightLastActivityMs = static_cast<uint32_t>(millis());
    _frontlight.setBrightness(_frontlightActiveBrightness);
}

}  // namespace platform::power
