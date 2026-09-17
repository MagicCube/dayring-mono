#include <cassert>
#include <iostream>
#include <vector>

#include "platform/hal/Frontlight.h"
#include "platform/hal/services/FrontlightService.h"

namespace {

std::vector<uint8_t> writes;

}

namespace platform::hal {

void testFrontlightBrightness(uint8_t percent) {
    writes.push_back(percent);
}

}  // namespace platform::hal

int main() {
    using platform::frontlight::FrontlightService;
    platform::hal::beginFrontlight();
    assert(platform::hal::frontlightBrightness() == 20);
    FrontlightService light;
    const auto bootWrites = writes.size();
    light.setBrightness(60);
    assert(light.start() && light.start());
    assert(writes.size() == bootWrites && light.brightness() == 20);
    light.setBrightness(60);
    light.setBrightness(60);
    assert(writes.size() == bootWrites + 1 && light.isOn());
    light.turnOff();
    assert(!light.isOn() && light.brightness() == 0);
    light.turnOn();
    assert(light.brightness() == 60);
    light.setBrightness(255);
    assert(light.brightness() == 100 && writes.back() == 100);
    const auto beforeStop = writes.size();
    light.stop();
    light.stop();
    light.turnOff();
    light.setBrightness(30);
    assert(!light.isRunning() && light.brightness() == 100 && writes.size() == beforeStop);
    assert(light.start());
    assert(writes.size() == beforeStop);
    light.turnOff();
    light.turnOn();
    assert(light.brightness() == 100);
    std::cout << "Frontlight boot handoff, brightness, switching and quiescence tests passed\n";
}
