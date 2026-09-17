#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "platform/hal/Frontlight.h"
#include "platform/hal/services/PowerService.h"

namespace {

uint32_t nowMs = 0;
bool advanceClock = false;
bool inputActive = false;
bool powerPressed = false;
uint8_t brightness = 0;
std::vector<uint8_t> writes;

}  // namespace

unsigned long millis() {
    return advanceClock ? nowMs++ : nowMs;
}

namespace platform::hal {

bool hasInputActivity() {
    return inputActive;
}

bool powerButtonPressed() {
    return powerPressed;
}

void testFrontlightBrightness(uint8_t percent) {
    brightness = percent;
    writes.push_back(percent);
}

}  // namespace platform::hal

void checkIdleStages(platform::power::PowerService& power) {
    nowMs += 51999;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 20);
    ++nowMs;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 10);
    const auto count = writes.size();
    nowMs += 7999;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 10 && writes.size() == count);
    assert(!power.isIdleLockDue());
    ++nowMs;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 0);
    inputActive = false;
    power.update(nowMs);
    assert(writes.size() == count + 1);
    assert(power.isIdleLockDue());
}

void testTimeoutAndActivity(platform::power::PowerService& power) {
    assert(brightness == 20);
    checkIdleStages(power);
    power.notifyActivity();
    assert(brightness == 20);
    nowMs += 52000;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 10);
    power.notifyActivity();
    assert(brightness == 20);
    checkIdleStages(power);
    // A delayed update can cross both thresholds at once.
    power.notifyActivity();
    nowMs += 70000;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 0);
}

void testLockAndRollover(platform::power::PowerService& power) {
    for (const uint32_t idleMs : {0U, 52000U, 60000U}) {
        power.notifyActivity();
        nowMs += idleMs;
        inputActive = false;
        power.update(nowMs);
        power.setLocked(true);
        assert(brightness == 0);
        const auto count = writes.size();
        power.notifyActivity();
        nowMs += 70000;
        inputActive = false;
        power.update(nowMs);
        assert(brightness == 0 && writes.size() == count);
        power.setLocked(false);
        assert(brightness == 20);
        checkIdleStages(power);
    }
    nowMs = std::numeric_limits<uint32_t>::max() - 1000;
    power.notifyActivity();
    checkIdleStages(power);
}

void testFirstLockLighting(platform::power::PowerService& power) {
    power.stop();
    assert(power.start());
    // Board initialization may take time; the grace period starts at the first lock.
    nowMs += 70000;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 0);
    power.setLocked(true);
    assert(brightness == 20);
    const auto count = writes.size();
    nowMs += 9999;
    power.notifyActivity();
    power.setLocked(true);
    inputActive = true;
    power.update(nowMs);
    assert(brightness == 20 && writes.size() == count);
    ++nowMs;
    inputActive = true;
    power.update(nowMs);
    assert(brightness == 0 && writes.size() == count + 1);
    nowMs += 70000;
    inputActive = true;
    power.update(nowMs);
    assert(brightness == 0 && writes.size() == count + 1);
    power.setLocked(false);
    assert(brightness == 20);
    power.setLocked(true);
    assert(brightness == 0);
    power.setLocked(false);
    checkIdleStages(power);
}

void testBootLighting(platform::power::PowerService& power) {
    power.stop();
    assert(power.start());
    // Boot feedback must be visible before the first update or any input.
    assert(brightness == 20);
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 20);
    checkIdleStages(power);
}

void testUnlockDuringFirstLock(platform::power::PowerService& power) {
    power.stop();
    assert(power.start());
    power.setLocked(true);
    nowMs += 5000;
    power.setLocked(false);
    nowMs += 5000;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 20);
    power.setLocked(true);
    assert(brightness == 0);
    power.setLocked(false);
    checkIdleStages(power);
}

int main() {
    platform::hal::beginFrontlight();
    assert(brightness == 20);
    platform::frontlight::FrontlightService frontlight;
    platform::power::PowerService power(frontlight);
    assert(!power.start());
    assert(!power.isRunning());
    assert(frontlight.start());
    power.stop();
    assert(power.start());
    nowMs += 60000;
    assert(power.isIdleLockDue());
    power.setLocked(true, platform::power::PowerService::LockReason::Idle);
    assert(brightness == 0 && !power.isIdleLockDue());
    power.setLocked(false);
    assert(brightness == 20 && !power.isIdleLockDue());
    power.setLocked(true);
    assert(brightness == 0);
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

    power.stop();
    assert(power.start());
    power.setLocked(true);
    nowMs += 52000;
    inputActive = true;
    power.update(nowMs);
    assert(brightness == 0);
    power.setLocked(false);
    assert(brightness == 20);
    checkIdleStages(power);

    power.stop();
    assert(power.start());
    power.setLocked(true);
    power.setLocked(false);
    assert(brightness == 20);
    power.stop();
    assert(power.start());
    nowMs += 90000;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 0);

    inputActive = true;
    power.update(nowMs);
    assert(brightness == 20);
    nowMs += 70000;
    inputActive = true;
    power.update(nowMs);
    assert(brightness == 20);
    checkIdleStages(power);
    power.stop();
    assert(power.start());
    nowMs += 1000;
    inputActive = true;
    power.update(nowMs);
    assert(brightness == 20);
    checkIdleStages(power);
    power.setLocked(true);
    power.notifyPowerConnectionChanged();
    assert(brightness == 20);
    nowMs += 4999;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 20);
    nowMs += 1;
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 0);
    // A clock tick between boot/activity and idle sampling must not underflow.
    advanceClock = true;
    power.stop();
    assert(power.start());
    inputActive = false;
    power.update(nowMs);
    assert(brightness == 20);
    inputActive = true;
    power.update(nowMs);
    assert(brightness == 20);
    advanceClock = false;
    const auto beforeStop = writes.size();
    power.stop();
    power.stop();
    nowMs += 100000;
    power.update(nowMs);
    power.notifyActivity();
    power.notifyPowerConnectionChanged();
    power.setLocked(true);
    assert(!power.isIdleLockDue() && writes.size() == beforeStop);
    assert(!power.isRunning());
    assert(power.start());
    power.setLocked(true);
    const auto lockedWrites = writes.size();
    assert(power.start());
    assert(writes.size() == lockedWrites);
    std::cout << "PowerService boot lighting, first-lock grace, idle, activity, lock, and rollover tests passed\n";
}
