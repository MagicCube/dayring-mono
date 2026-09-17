#include "FrameBuffer.h"

#include <iostream>

namespace preview {

std::array<uint8_t, 800 * 480 / 8>& framePixels() {
    static std::array<uint8_t, 800 * 480 / 8> pixels;
    return pixels;
}

std::array<uint8_t, 800 * 480>& grayPixels() {
    static std::array<uint8_t, 800 * 480> pixels;
    return pixels;
}

void resetFrame() {
    grayPixels().fill(255);
    framePixels().fill(0xFF);
}

std::vector<uint8_t> portraitPixels() {
    std::vector<uint8_t> result(480 * 800);
    for (int y = 0; y < 800; ++y) {
        for (int x = 0; x < 480; ++x) {
            // Inverse of DisplayTarget's Portrait logical-to-native mapping.
            const int nativeX = y;
            const int nativeY = 479 - x;
            result[y * 480 + x] = grayPixels()[nativeY * 800 + nativeX];
        }
    }
    return result;
}

int writeFrame(const char* extraJson) {
    const auto pixels = portraitPixels();
    std::cout << "{\"protocol\":1,\"width\":480,\"height\":800,\"format\":\"gray8\"" << extraJson << "}\n";
    std::cout.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    return std::cout ? 0 : 5;
}

}  // namespace preview
