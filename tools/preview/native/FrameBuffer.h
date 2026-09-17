#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace preview {

std::array<uint8_t, 800 * 480 / 8>& framePixels();
std::array<uint8_t, 800 * 480>& grayPixels();
void resetFrame();
std::vector<uint8_t> portraitPixels();
int writeFrame(const char* extraJson);

}  // namespace preview
