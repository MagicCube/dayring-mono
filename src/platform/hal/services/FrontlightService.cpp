#include "FrontlightService.h"

#include <algorithm>

#include "../Frontlight.h"

namespace platform::frontlight {

bool FrontlightService::start() {
    if (_running) return true;
    if (const auto current = brightness(); current != 0) _lastOnBrightness = current;
    _running = true;
    return true;
}

void FrontlightService::stop() {
    // Quiesce control without changing the display supply or current lighting.
    _running = false;
}

bool FrontlightService::isRunning() const {
    return _running;
}

uint8_t FrontlightService::brightness() const {
    return hal::frontlightBrightness();
}

bool FrontlightService::isOn() const {
    return brightness() != 0;
}

void FrontlightService::setBrightness(uint8_t percent) {
    if (!_running) return;
    percent = std::min<uint8_t>(percent, 100);
    if (percent != 0) _lastOnBrightness = percent;
    hal::setFrontlightBrightness(percent);
}

void FrontlightService::turnOn() {
    setBrightness(_lastOnBrightness);
}

void FrontlightService::turnOff() {
    setBrightness(0);
}

}  // namespace platform::frontlight
