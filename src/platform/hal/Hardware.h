#pragma once

#include <cstdint>
#include <span>

class InputManager;

namespace platform::hal {
class PowerManager;
[[nodiscard]] PowerManager& powerManager();
// Touch activity and ordinary button edges/holds from the latest input sample.
[[nodiscard]] bool hasInputActivity();
// Non-owning view of the HAL-owned monochrome buffer. Valid after begin();
// render only while displayReady() is true. Each row contains strideBytes bytes.
struct Framebuffer {
    std::span<uint8_t> pixels;
    uint16_t width;
    uint16_t height;
    uint16_t strideBytes;
};

void begin();
void update();
[[nodiscard]] Framebuffer framebuffer();
[[nodiscard]] const InputManager& input();
// True only for the update in which the SDK reports a power-button press edge.
[[nodiscard]] bool powerButtonPressed();
// Completed tap in normalized panel-native coordinates; false for drags.
[[nodiscard]] bool touchTapped(float& x, float& y);
// Completed single-contact swipe in normalized panel-native coordinates.
[[nodiscard]] bool touchSwiped(float& startX, float& startY, float& endX, float& endY);
[[nodiscard]] bool displayReady();
void refreshDisplay();
[[nodiscard]] bool readBatteryPercent(uint8_t& percent);
[[nodiscard]] bool readCharging(bool& charging);
[[noreturn]] void fatal();
}  // namespace platform::hal
