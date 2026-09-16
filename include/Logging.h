#pragma once

#include <Arduino.h>

// This filename and macro are required by FreeInk's FrontlightManager.
// Keep the consumer-side adapter here rather than modifying the SDK.
namespace FreeInkCompat {

template <typename... Args>
void info(const char* tag, const char* format, Args... args) {
    Serial.printf("[%s] ", tag);
    Serial.printf(format, args...);
    Serial.println();
}

}  // namespace FreeInkCompat

#define LOG_INF(...) ::FreeInkCompat::info(__VA_ARGS__)
