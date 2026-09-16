#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "platform/hal/PowerManager.h"

namespace {
uint32_t nowMs = 0;
bool advanceClock = false;
uint8_t brightness = 0;
std::vector<uint8_t> writes;
}  // namespace

unsigned long millis() {
    return advanceClock ? nowMs++ : nowMs;
}

namespace platform::hal {
void testFrontlightBrightness(uint8_t percent) {
    brightness = percent;
    writes.push_back(percent);
}
}  // namespace platform::hal

void checkIdleStages(platform::hal::PowerManager& power) {
    nowMs += 51999;
    power.update();
    assert(brightness == 20);
    ++nowMs;
    power.update();
    assert(brightness == 10);
    const auto count = writes.size();
    nowMs += 7999;
    power.update();
    assert(brightness == 10 && writes.size() == count);
    ++nowMs;
    power.update();
    assert(brightness == 0);
    power.update();
    assert(writes.size() == count + 1);
}

void testTimeoutAndActivity(platform::hal::PowerManager& power) {
    assert(brightness == 20);
    checkIdleStages(power);
    power.notifyActivity();
    assert(brightness == 20);
    nowMs += 52000;
    power.update();
    assert(brightness == 10);
    power.notifyActivity();
    assert(brightness == 20);
    checkIdleStages(power);
    // A delayed update can cross both thresholds at once.
    power.notifyActivity();
    nowMs += 70000;
    power.update();
    assert(brightness == 0);
}

void testLockAndRollover(platform::hal::PowerManager& power) {
    for (const uint32_t idleMs : {0U, 52000U, 60000U}) {
        power.notifyActivity();
        nowMs += idleMs;
        power.update();
        power.setLocked(true);
        assert(brightness == 0);
        const auto count = writes.size();
        power.notifyActivity();
        nowMs += 70000;
        power.update();
        assert(brightness == 0 && writes.size() == count);
        power.setLocked(false);
        assert(brightness == 20);
        checkIdleStages(power);
    }
    nowMs = std::numeric_limits<uint32_t>::max() - 1000;
    power.notifyActivity();
    checkIdleStages(power);
}

void testFirstLockLighting(platform::hal::PowerManager& power) {
    power.begin();
    // Board initialization may take time; the grace period starts at the first lock.
    nowMs += 70000;
    power.update();
    assert(brightness == 0);
    power.setLocked(true);
    assert(brightness == 20);
    const auto count = writes.size();
    nowMs += 9999;
    power.notifyActivity();
    power.setLocked(true);
    power.update(true);
    assert(brightness == 20 && writes.size() == count);
    ++nowMs;
    power.update(true);
    assert(brightness == 0 && writes.size() == count + 1);
    nowMs += 70000;
    power.update(true);
    assert(brightness == 0 && writes.size() == count + 1);
    power.setLocked(false);
    assert(brightness == 20);
    power.setLocked(true);
    assert(brightness == 0);
    power.setLocked(false);
    checkIdleStages(power);
}

void testBootLighting(platform::hal::PowerManager& power) {
    power.begin();
    // Boot feedback must be visible before the first update or any input.
    assert(brightness == 20);
    power.update();
    assert(brightness == 20);
    checkIdleStages(power);
}

void testUnlockDuringFirstLock(platform::hal::PowerManager& power) {
    power.begin();
    power.setLocked(true);
    nowMs += 5000;
    power.setLocked(false);
    nowMs += 5000;
    power.update();
    assert(brightness == 20);
    power.setLocked(true);
    assert(brightness == 0);
    power.setLocked(false);
    checkIdleStages(power);
}

int main() {
    platform::hal::PowerManager power;
    testBootLighting(power);
    nowMs = std::numeric_limits<uint32_t>::max() - 1000;
    testBootLighting(power);
    power.notifyActivity();
    testTimeoutAndActivity(power);
    testFirstLockLighting(power);
    nowMs = std::numeric_limits<uint32_t>::max() - 75000;
    testFirstLockLighting(power);
    testLockAndRollover(power);
    testUnlockDuringFirstLock(power);

    power.begin();
    power.setLocked(true);
    nowMs += 52000;
    power.update(true);
    assert(brightness == 0);
    power.setLocked(false);
    assert(brightness == 20);
    checkIdleStages(power);

    power.begin();
    power.setLocked(true);
    power.setLocked(false);
    assert(brightness == 20);
    power.begin();
    nowMs += 90000;
    power.update();
    assert(brightness == 0);

    power.update(true);
    assert(brightness == 20);
    nowMs += 70000;
    power.update(true);
    assert(brightness == 20);
    checkIdleStages(power);
    power.begin();
    nowMs += 1000;
    power.update(true);
    assert(brightness == 20);
    checkIdleStages(power);
    // A clock tick between boot/activity and idle sampling must not underflow.
    advanceClock = true;
    power.begin();
    power.update();
    assert(brightness == 20);
    power.update(true);
    assert(brightness == 20);
    advanceClock = false;
    std::cout << "PowerManager boot lighting, first-lock grace, idle, activity, lock, and rollover tests passed\n";
}
