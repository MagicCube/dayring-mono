#include "Frontlight.h"

#include <FrontlightManager.h>

#include <algorithm>

namespace platform::hal {
namespace {

FrontlightManager driver;
uint8_t brightness = 0;
bool initialized = false;

}  // namespace

void beginFrontlight() {
    if (initialized) return;
    driver.begin();
    driver.off();
    initialized = true;
    // Boot feedback precedes display initialization and runtime service startup.
    setFrontlightBrightness(20);
}

void setFrontlightBrightness(uint8_t percent) {
    percent = std::min<uint8_t>(percent, 100);
    if (brightness == percent) return;
    driver.setBrightness(percent);
    brightness = percent;
}

uint8_t frontlightBrightness() {
    return brightness;
}

}  // namespace platform::hal
