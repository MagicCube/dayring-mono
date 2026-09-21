#pragma once
#include <Rtc.h>

#include <chrono>
#include <optional>
#include <span>

namespace platform::time {

// UTC epoch seconds (u64 LE), followed by current UTC offset seconds (i32 LE).
inline std::optional<Rtc::DateTime> decodeClockSample(std::span<const uint8_t> bytes) {
    if (bytes.size() != 12) return std::nullopt;
    uint64_t epoch = 0;
    uint32_t rawOffset = 0;
    for (int i = 0; i < 8; ++i) epoch |= uint64_t(bytes[i]) << (8 * i);
    for (int i = 0; i < 4; ++i) rawOffset |= uint32_t(bytes[i + 8]) << (8 * i);
    const int64_t offset = rawOffset <= 0x7FFFFFFFU ? rawOffset : int64_t(rawOffset) - 0x100000000LL;
    if (epoch < 946684800ULL || epoch > 4102444799ULL || offset < -50400 || offset > 50400) return std::nullopt;
    using namespace std::chrono;
    const sys_seconds local{seconds{static_cast<int64_t>(epoch) + offset}};
    const auto dayPoint = floor<days>(local);
    const year_month_day date{dayPoint};
    const hh_mm_ss clock{local - dayPoint};
    const int year = int(date.year());
    if (year < 2000 || year > 2099) return std::nullopt;
    return Rtc::DateTime{.year = static_cast<uint16_t>(year),
                         .month = static_cast<uint8_t>(unsigned(date.month())),
                         .day = static_cast<uint8_t>(unsigned(date.day())),
                         .hour = static_cast<uint8_t>(clock.hours().count()),
                         .minute = static_cast<uint8_t>(clock.minutes().count()),
                         .second = static_cast<uint8_t>(clock.seconds().count()),
                         .weekday = static_cast<uint8_t>(weekday{dayPoint}.c_encoding())};
}

}  // namespace platform::time
