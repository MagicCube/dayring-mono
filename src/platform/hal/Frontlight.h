#pragma once

#include <cstdint>

namespace platform::hal {

void beginFrontlight();
void setFrontlightBrightness(uint8_t percent);
[[nodiscard]] uint8_t frontlightBrightness();

}  // namespace platform::hal
