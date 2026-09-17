#include "HostHardware.h"

#include <array>
#include <cstdlib>

#include "platform/hal/Hardware.h"
#include "platform/hal/PowerManager.h"
#include "platform/hal/RtcClock.h"

namespace {
// Match Hardware::begin's required PaperMono native framebuffer geometry.
std::array<uint8_t, 800 * 480 / 8> pixels;
Rtc::DateTime clockValue{.hour = 12, .minute = 34};
uint8_t batteryValue = 75;
bool chargingValue = false;
bool submitted = false;
}  // namespace

unsigned long millis() {
    return 0;
}

namespace preview {
void configureHardware(uint8_t hour, uint8_t minute, uint8_t battery, bool charging) {
    clockValue.hour = hour;
    clockValue.minute = minute;
    batteryValue = battery;
    chargingValue = charging;
    pixels.fill(0xFF);
    submitted = false;
    platform::hal::powerManager().begin();
}

bool hasFrame() {
    return submitted;
}

std::vector<uint8_t> portraitPixels() {
    std::vector<uint8_t> result(480 * 800);
    for (int y = 0; y < 800; ++y) {
        for (int x = 0; x < 480; ++x) {
            // Inverse of DisplayTarget's Portrait logical-to-native mapping.
            const int nativeX = y;
            const int nativeY = 479 - x;
            const bool white = (pixels[nativeY * 100 + nativeX / 8] & (0x80 >> (nativeX % 8))) != 0;
            result[y * 480 + x] = white ? 255 : 0;
        }
    }
    return result;
}
}  // namespace preview

namespace platform::hal {
PowerManager& powerManager() {
    static PowerManager manager;
    return manager;
}
Framebuffer framebuffer() {
    return {pixels, 800, 480, 100};
}
bool displayReady() {
    return true;
}
void refreshDisplay() {
    submitted = true;
}
Rtc::DateTime clockTime() {
    return clockValue;
}
bool readBatteryPercent(uint8_t& percent) {
    percent = batteryValue;
    return true;
}
bool readCharging(bool& charging) {
    charging = chargingValue;
    return true;
}
bool powerButtonPressed() {
    return false;
}
bool hasInputActivity() {
    return false;
}
bool touchTapped(float&, float&) {
    return false;
}
bool touchSwiped(float&, float&, float&, float&) {
    return false;
}
[[noreturn]] void fatal() {
    std::abort();
}
}  // namespace platform::hal
