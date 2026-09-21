#pragma once

#include <Rtc.h>

namespace platform::hal {

void initializeRtc();
[[nodiscard]] bool setClockTime(const Rtc::DateTime& value);
[[nodiscard]] Rtc::DateTime clockTime();

}  // namespace platform::hal
