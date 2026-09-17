#pragma once

#include <algorithm>
#include <span>

#include "generated/BootImage.h"

namespace platform::hal::boot {

// Compose one native row from the centered 1-bit portrait asset.
inline void composeRow(int nativeY, std::span<uint8_t> base) {
    std::fill(base.begin(), base.end(), 0);
    const int x = 479 - nativeY - (480 - width) / 2;
    if (x < 0 || x >= width) return;
    for (int y = 0; y < height; ++y) {
        const auto level = (pixels[y * stride + x / 8] >> (7 - x % 8)) & 1;
        const int nativeX = (800 - height) / 2 + y;
        const auto mask = static_cast<uint8_t>(0x80 >> (nativeX % 8));
        const int offset = nativeX / 8;
        if (level) base[offset] |= mask;
    }
}

}  // namespace platform::hal::boot
