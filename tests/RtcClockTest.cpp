#include <cassert>
#include <cstdlib>
#include <iostream>

#include "platform/hal/RtcClock.h"

namespace {

Rtc::DateTime stored{.year = 2026, .month = 9, .day = 21, .hour = 13, .minute = 42, .weekday = 1};
bool valid = true;
unsigned writes = 0;

}  // namespace

namespace freeink {

bool Rtc::begin() {
    return true;
}

bool Rtc::now(DateTime& value) {
    value = stored;
    return valid;
}

bool Rtc::set(const DateTime& value) {
    stored = value;
    valid = true;
    ++writes;
    return true;
}

}  // namespace freeink

namespace platform::hal {

[[noreturn]] void fatal() {
    std::abort();
}

}  // namespace platform::hal

int main() {
    platform::hal::initializeRtc();
    platform::hal::initializeRtc();
    assert(writes == 0);
    assert(platform::hal::clockTime().hour == 13 && stored.minute == 42);
    valid = false;
    platform::hal::initializeRtc();
    assert(writes == 1 && stored.year == 2000 && stored.month == 1 && stored.day == 1);
    assert(stored.hour == 0 && stored.minute == 0 && stored.weekday == 6);
    const Rtc::DateTime synchronized{.year = 2026, .month = 9, .day = 21, .hour = 14, .weekday = 1};
    assert(platform::hal::setClockTime(synchronized));
    platform::hal::initializeRtc();
    assert(writes == 2 && platform::hal::clockTime().hour == 14);
    std::cout << "RTC startup preserves existing/synchronized time and seeds only an invalid clock\n";
}
