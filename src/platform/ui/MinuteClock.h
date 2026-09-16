#pragma once

#include <Arduino.h>

#include "../hal/RtcClock.h"

namespace platform::ui {
// Keep the displayed minute stable until the RTC reaches second 01.
class MinuteClock {
   public:
    void reset() {
        _initialized = false;
    }
    [[nodiscard]] bool update() {
        const uint32_t now = millis();
        if (_initialized && now - _sampledAt < _intervalMs) return false;
        const auto sample = hal::clockTime();
        _sampledAt = now;
        _intervalMs = (sample.second == 0 ? 1U : 61U - sample.second) * 1000U;
        if (_initialized && sample.second == 0) return false;
        const bool changed = !_initialized || sample.hour != _time.hour || sample.minute != _time.minute;
        _initialized = true;
        _time = sample;
        return changed;
    }
    [[nodiscard]] const Rtc::DateTime& time() const {
        return _time;
    }

   private:
    Rtc::DateTime _time{};
    uint32_t _sampledAt = 0;
    uint32_t _intervalMs = 0;
    bool _initialized = false;
};
}  // namespace platform::ui
