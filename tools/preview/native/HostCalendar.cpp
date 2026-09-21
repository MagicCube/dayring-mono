#include "HostCalendar.h"

#include <chrono>
#include <cstdio>

#include "platform/calendar/CalendarStorage.h"
#include "platform/hal/RtcClock.h"

namespace {

std::string scenario = "empty";

std::string timestamp(std::chrono::sys_days date, std::string_view time) {
    const std::chrono::year_month_day day{date};
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "%04d-%02u-%02uT", int(day.year()), unsigned(day.month()), unsigned(day.day()));
    return prefix + std::string(time) + ":00Z";
}

std::string event(std::string_view id, std::string_view title, std::string_view location, std::string_view start,
                  std::string_view end, bool allDay = false) {
    return "{\"instanceId\":\"" + std::string(id) + "\",\"title\":\"" + std::string(title) + "\",\"location\":\"" +
           std::string(location) + "\",\"start\":\"" + std::string(start) + "\",\"end\":\"" + std::string(end) +
           "\",\"isAllDay\":" + (allDay ? "true" : "false") + "}";
}

class MockCalendarStorage final : public platform::calendar::CalendarStorage {
   public:
    std::optional<std::string> load() override {
        if (scenario == "empty") return std::nullopt;
        const auto clock = platform::hal::clockTime();
        using namespace std::chrono;
        const sys_days today{year{clock.year} / month{clock.month} / day{clock.day}};
        const auto tomorrow = today + days{1};
        std::string events;
        if (scenario == "all-day") {
            events = event("all-day", "Amy's Birthday", "", timestamp(tomorrow, "00:00"),
                           timestamp(today + days{2}, "00:00"), true) +
                     ",";
        }
        events +=
            event("past", "Earlier meeting", "Office", timestamp(today, "10:00"), timestamp(today, "11:00")) + ",";
        const auto title = scenario == "long-title" ? "A Very Long Title Test - if any" : "Hangout with Siyu";
        const auto location = scenario == "long-title" ? "Skytown, Building A, Meeting Room 1208" : "FanCity";
        events += event("today-1", title, location, timestamp(today, "14:00"), timestamp(today, "15:00")) + ",";
        events += event("today-2", "Dinner Time", "Home", timestamp(today, "18:00"), timestamp(today, "19:00")) + ",";
        events += event("tomorrow-all-day", "Tomorrow holiday", "", timestamp(tomorrow, "00:00"),
                        timestamp(today + days{2}, "00:00"), true) +
                  ",";
        events += event("tomorrow-1", "Morning catch-up", "Corner Cafe", timestamp(tomorrow, "09:00"),
                        timestamp(tomorrow, "10:00")) +
                  ",";
        events +=
            event("tomorrow-2", "Project review", "Studio", timestamp(tomorrow, "11:00"), timestamp(tomorrow, "12:00"));
        return "{\"schemaVersion\":1,\"generatedAt\":\"" + timestamp(today, "00:00") +
               "\",\"timeZone\":\"Etc/UTC\",\"windowStart\":\"" + timestamp(today, "00:00") +
               "\",\"windowEndExclusive\":\"" + timestamp(today + days{2}, "00:00") + "\",\"events\":[" + events + "]}";
    }

    bool save(std::string_view) override {
        return true;
    }
};

}  // namespace

namespace preview {

bool configureCalendar(std::string_view value) {
    if (value != "empty" && value != "sample" && value != "long-title" && value != "all-day") return false;
    scenario = value;
    return true;
}

}  // namespace preview

namespace platform::calendar {

std::unique_ptr<CalendarStorage> makeCalendarStorage() {
    return std::make_unique<MockCalendarStorage>();
}

}  // namespace platform::calendar
