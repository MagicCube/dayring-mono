#include <cassert>
#include <iostream>
#include <limits>

#include "platform/hal/RtcClock.h"
#include "platform/time/services/TimeService.h"

namespace {

uint32_t nowMs = 0;
unsigned reads = 0;
Rtc::DateTime rtcValue{};

}  // namespace

unsigned long millis() {
    return nowMs;
}

namespace platform::hal {

bool setClockTime(const Rtc::DateTime& value) {
    rtcValue = value;
    return true;
}

Rtc::DateTime clockTime() {
    ++reads;
    return rtcValue;
}

}  // namespace platform::hal

int main() {
    using platform::time::TimeService;
    TimeService service;
    service.update(1000);
    assert(reads == 0);
    rtcValue = {.year = 2026, .month = 12, .day = 31, .hour = 23, .minute = 59, .second = 59, .weekday = 4};
    assert(service.start() && service.start());
    assert(reads == 1);
    const auto initialRevision = service.minuteRevision();
    assert(initialRevision == 1);
    assert(service.minuteRevision() == initialRevision && reads == 1);
    nowMs = 999;
    service.update(nowMs);
    assert(reads == 1);
    nowMs = 1000;
    rtcValue = {.year = 2027, .month = 1, .day = 1, .hour = 0, .minute = 0, .second = 0, .weekday = 5};
    service.update(nowMs);
    assert(service.time().year == 2027);
    assert(service.minuteRevision() == initialRevision);
    assert(service.displayTime().year == 2026 && service.displayTime().minute == 59);
    nowMs = 2000;
    rtcValue.second = 1;
    service.update(nowMs);
    assert(service.minuteRevision() == initialRevision + 1);
    assert(service.displayTime().year == 2027 && service.displayTime().minute == 0);
    const auto sampled = reads;
    (void)service.displayTime();
    (void)service.time();
    assert(reads == sampled);
    nowMs = 65000;
    rtcValue.minute = 1;
    rtcValue.second = 4;
    service.update(nowMs);
    assert(service.displayTime().minute == rtcValue.minute);
    // A date correction with the same hour/minute still invalidates displays.
    nowMs += 1000;
    rtcValue.day = 2;
    rtcValue.weekday = 6;
    service.update(nowMs);
    assert(service.minuteRevision() == initialRevision + 3 && service.displayTime().day == 2);
    nowMs += 1000;
    rtcValue.hour = 23;
    rtcValue.minute = 58;
    service.update(nowMs);
    assert(service.minuteRevision() == initialRevision + 4 && service.displayTime().hour == 23);
    const auto beforeStop = reads;
    service.stop();
    service.stop();
    service.update(nowMs + 10000);
    assert(reads == beforeStop && !service.isRunning());
    assert(service.time().hour == 23);
    nowMs = std::numeric_limits<uint32_t>::max() - 500;
    assert(service.start());
    assert(service.displayTime().minute == rtcValue.minute);
    rtcValue.minute = 59;
    nowMs = 499;
    service.update(nowMs);
    assert(service.displayTime().minute == rtcValue.minute);
    assert(service.displayTime().minute == 59);
    std::cout << "Shared time, rollover, corrections, display revisions and lifecycle tests passed\n";
}
