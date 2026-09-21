#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace platform::time {

class TimeService;

}

namespace platform::calendar {

struct ClockSample {
    std::optional<int64_t> localSeconds;
    std::optional<int32_t> utcOffsetSeconds;
    std::string timeZone;
    bool operator==(const ClockSample&) const = default;
};

[[nodiscard]] ClockSample sampleClock(const time::TimeService& time);

}  // namespace platform::calendar
