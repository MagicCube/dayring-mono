#pragma once
#include <span>
#include <string>

#include "../../../platform/ui/Page.h"

namespace apps::shell::pages {

struct LockCalendarItem {
    std::string title;
    std::string location;
    std::string startTime;
    std::string endTime;
    bool isTomorrow = false;
    bool isAllDay = false;
    bool operator==(const LockCalendarItem&) const = default;
};

struct LockPageProps {
    uint8_t hour = 12;
    uint8_t minute = 34;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t weekday = 0;
    bool charging = false;
    bool batteryKnown = false;
    uint8_t percent = 0;
    bool showUnlockHint = false;
    std::span<const LockCalendarItem> events{};
};

class LockPage final : public platform::ui::Page<LockPageProps> {
   public:
    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override;

   private:
    void _renderCalendar(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const;
};

}  // namespace apps::shell::pages
