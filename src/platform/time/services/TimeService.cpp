#include "TimeService.h"

#include <Arduino.h>

#include "../../hal/RtcClock.h"

namespace platform::time {

bool TimeService::start() {
    if (_running) return true;
    _sample(static_cast<uint32_t>(millis()), true);
    _running = true;
    return true;
}

void TimeService::stop() {
    _running = false;
}

void TimeService::update(uint32_t now) {
    if (_running && now - _sampledAt >= 1000U) _sample(now, false);
}

void TimeService::_sample(uint32_t now, bool initial) {
    _time = hal::clockTime();
    _sampledAt = now;
    // Preserve the existing display policy: advance the minute at second 01.
    if (!initial && _time.second == 0) return;
    if (initial || _time.year != _displayTime.year || _time.month != _displayTime.month ||
        _time.day != _displayTime.day || _time.weekday != _displayTime.weekday || _time.hour != _displayTime.hour ||
        _time.minute != _displayTime.minute) {
        _displayTime = _time;
        ++_minuteRevision;
    }
}

bool TimeService::isRunning() const {
    return _running;
}

const Rtc::DateTime& TimeService::time() const {
    return _time;
}

const Rtc::DateTime& TimeService::displayTime() const {
    return _displayTime;
}

uint64_t TimeService::minuteRevision() const {
    return _minuteRevision;
}

}  // namespace platform::time
