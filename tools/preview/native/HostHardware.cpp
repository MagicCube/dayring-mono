#include "HostHardware.h"

#include <array>
#include <cstdlib>
#include <ctime>

#include "FrameBuffer.h"
#include "platform/hal/Frontlight.h"
#include "platform/hal/Hardware.h"
#include "platform/hal/RtcClock.h"

namespace {

// Match Hardware::begin's required PaperMono native framebuffer geometry.

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
    const auto now = std::time(nullptr);
    const auto local = *std::localtime(&now);
    clockValue.year = static_cast<uint16_t>(local.tm_year + 1900);
    clockValue.month = static_cast<uint8_t>(local.tm_mon + 1);
    clockValue.day = static_cast<uint8_t>(local.tm_mday);
    clockValue.weekday = static_cast<uint8_t>(local.tm_wday);
    clockValue.hour = hour;
    clockValue.minute = minute;
    batteryValue = battery;
    chargingValue = charging;
    resetFrame();
    submitted = false;
    platform::hal::beginFrontlight();
}

bool hasFrame() {
    return submitted;
}

}  // namespace preview

namespace platform::hal {

Framebuffer framebuffer() {
    return {preview::framePixels(), 800, 480, 100, preview::grayPixels()};
}

bool displayReady() {
    return true;
}

void refreshDisplay() {
    submitted = true;
}

bool setClockTime(const Rtc::DateTime& value) {
    clockValue = value;
    return true;
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
