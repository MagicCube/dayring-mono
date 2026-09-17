#pragma once

#include <cstdint>
#include <vector>

namespace preview {

void configureHardware(uint8_t hour, uint8_t minute, uint8_t battery, bool charging);
[[nodiscard]] bool hasFrame();
[[nodiscard]] std::vector<uint8_t> portraitPixels();

}  // namespace preview
