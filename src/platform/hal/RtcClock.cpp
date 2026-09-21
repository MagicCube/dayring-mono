#include "RtcClock.h"

#include "Hardware.h"

namespace platform::hal {
namespace {

Rtc rtc;

}  // namespace

void initializeRtc() {
    if (!rtc.begin()) fatal();
    Rtc::DateTime now;
    if (rtc.now(now)) return;
    // An invalid RTC needs a valid baseline before BLE is available. RPC supplies real time.
    constexpr Rtc::DateTime baseline{.year = 2000, .month = 1, .day = 1, .weekday = 6};
    if (!rtc.set(baseline) || !rtc.now(now)) fatal();
}

bool setClockTime(const Rtc::DateTime& value) {
    return rtc.set(value);
}

Rtc::DateTime clockTime() {
    Rtc::DateTime now;
    if (!rtc.now(now)) fatal();
    return now;
}

}  // namespace platform::hal
