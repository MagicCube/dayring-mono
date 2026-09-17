#pragma once

#include <cstdint>

// The host has no frontlight. Keep production power policy linked and inert.
class FrontlightManager {
   public:
    void begin() {
    }

    void off() {
    }

    void setBrightness(uint8_t) {
    }
};
