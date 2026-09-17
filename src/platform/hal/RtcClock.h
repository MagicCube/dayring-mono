#pragma once

#include <Rtc.h>

namespace platform::hal {

void initializeRtc();
[[nodiscard]] Rtc::DateTime clockTime();

}  // namespace platform::hal
