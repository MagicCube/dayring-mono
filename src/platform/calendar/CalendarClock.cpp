#include "CalendarClock.h"

#include <chrono>

#include "../time/services/TimeService.h"

namespace platform::calendar {

ClockSample sampleClock(const time::TimeService& time) {
    ClockSample sample;
    if (!time.isRunning()) return sample;
    const auto& value = time.time();
    using namespace std::chrono;
    const year_month_day date{year{value.year}, month{value.month}, day{value.day}};
    if (value.year < 2000 || value.year > 2099 || !date.ok() || value.hour > 23 || value.minute > 59 ||
        value.second > 59)
        return sample;
    sample.localSeconds = duration_cast<seconds>(sys_days{date}.time_since_epoch()).count() + value.hour * 3600 +
                          value.minute * 60 + value.second;
    if (time.hasTimeZone()) {
        sample.timeZone = time.timeZone();
        sample.utcOffsetSeconds = time.utcOffsetSeconds();
    }
    return sample;
}

}  // namespace platform::calendar
