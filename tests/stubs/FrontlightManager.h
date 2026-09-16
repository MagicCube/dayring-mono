#pragma once
#include <cstdint>

namespace platform::hal {
void testFrontlightBrightness(uint8_t percent);
}

class FrontlightManager {
   public:
    void begin() {
    }
    void off() {
        setBrightness(0);
    }
    void setBrightness(uint8_t percent) {
        platform::hal::testFrontlightBrightness(percent);
    }
};
