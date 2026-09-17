#include "RtcClock.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cstring>
#include <upload-rtc-config.hpp>

#include "Hardware.h"

namespace platform::hal {
namespace {

Rtc rtc;

uint8_t monthFromBuildDate(const char* date) {
    static constexpr const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (uint8_t i = 0; i < 12; ++i) {
        if (strncmp(date, kMonths[i], 3) == 0) return static_cast<uint8_t>(i + 1);
    }
    return 1;
}

uint8_t weekdayFor(uint16_t year, uint8_t month, uint8_t day) {
    static constexpr uint8_t kOffsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t y = year;
    if (month < 3) --y;
    return static_cast<uint8_t>((y + y / 4 - y / 100 + y / 400 + kOffsets[month - 1] + day) % 7);
}

Rtc::DateTime buildDateTime() {
    Rtc::DateTime value;
    value.month = monthFromBuildDate(__DATE__);
    value.day = static_cast<uint8_t>((__DATE__[4] == ' ' ? 0 : __DATE__[4] - '0') * 10 + (__DATE__[5] - '0'));
    value.year = static_cast<uint16_t>((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 +
                                       (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0'));
    value.hour = static_cast<uint8_t>((__TIME__[0] - '0') * 10 + (__TIME__[1] - '0'));
    value.minute = static_cast<uint8_t>((__TIME__[3] - '0') * 10 + (__TIME__[4] - '0'));
    value.second = static_cast<uint8_t>((__TIME__[6] - '0') * 10 + (__TIME__[7] - '0'));
    value.weekday = weekdayFor(value.year, value.month, value.day);
    return value;
}

#if PAPERMONO_UPLOAD_RTC_SYNC
Rtc::DateTime uploadRtcDateTime() {
    uint64_t packed = PAPERMONO_UPLOAD_RTC_STAMP;
    Rtc::DateTime value;
    value.second = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.minute = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.hour = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.day = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.month = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.year = static_cast<uint16_t>(packed);
    value.weekday = weekdayFor(value.year, value.month, value.day);
    return value;
}

bool applyUploadRtcSync(Rtc& rtc) {
    static constexpr const char* kPreferencesNamespace = "paper-mono";
    static constexpr const char* kStampKey = "rtc-stamp";
    static constexpr uint64_t kUploadStamp = PAPERMONO_UPLOAD_RTC_STAMP;

    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, false)) {
        return false;
    }

    if (preferences.getULong64(kStampKey, 0) == kUploadStamp) {
        preferences.end();
        return true;
    }

    const Rtc::DateTime target = uploadRtcDateTime();
    Rtc::DateTime readback;
    const bool set = rtc.set(target) && rtc.now(readback);
    const bool remembered = set && preferences.putULong64(kStampKey, kUploadStamp) == sizeof(uint64_t);
    preferences.end();

    return set && remembered;
}
#endif

}  // namespace

void initializeRtc() {
    if (!rtc.begin()) fatal();
#if PAPERMONO_UPLOAD_RTC_SYNC
    if (!applyUploadRtcSync(rtc)) fatal();
#endif
    Rtc::DateTime now;
    if (!rtc.now(now) && !(rtc.set(buildDateTime()) && rtc.now(now))) fatal();
}

Rtc::DateTime clockTime() {
    Rtc::DateTime now;
    if (!rtc.now(now)) fatal();
    return now;
}

}  // namespace platform::hal
