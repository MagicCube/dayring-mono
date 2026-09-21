#include "LockPage.h"

#include <algorithm>
#include <cstdio>

#include "../../../platform/fonts/Fonts.h"

namespace apps::shell::pages {
namespace {

int16_t gridLineHeight(platform::ui::Canvas& canvas, freeink::ui::FontId font) {
    return static_cast<int16_t>((canvas.lineHeight(font) + 3) / 4 * 4);
}

void calendarRow(platform::ui::Canvas& canvas, const platform::ui::Rect& row, const LockCalendarItem& event,
                 int16_t titleHeight, int16_t detailHeight) {
    using namespace freeink::ui;
    using platform::fonts::Font;
    using platform::fonts::fontId;
    constexpr int16_t ruleWidth = 6, textGap = 16, lineGap = 4;
    const int16_t textX = row.x + ruleWidth + textGap;
    const int16_t textWidth = row.width - ruleWidth - textGap;
    canvas.fill({row.x, static_cast<int16_t>(row.y + 4), ruleWidth, static_cast<int16_t>(row.height - 8)},
                Paint::solid(Color::White), ruleWidth / 2);
    canvas.text({textX, row.y, textWidth, titleHeight}, event.title.c_str(),
                {.font = fontId(Font::RobotoL), .color = Color::White, .maxLines = 1});
    canvas.text({textX, static_cast<int16_t>(row.y + titleHeight + lineGap), textWidth, detailHeight},
                event.time.c_str(), {.font = fontId(Font::RobotoM), .color = Color::LightGray, .maxLines = 1});
}

}  // namespace

void LockPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props) const {
    char time[6];
    snprintf(time, sizeof(time), "%02u:%02u", static_cast<unsigned>(props.hour), static_cast<unsigned>(props.minute));
    canvas.fill(bounds, freeink::ui::Paint::solid(freeink::ui::Color::Black));
    constexpr const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    constexpr const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char date[24];
    snprintf(date, sizeof(date), "%s %u %s", weekdays[props.weekday % 7], static_cast<unsigned>(props.day),
             months[props.month >= 1 && props.month <= 12 ? props.month - 1 : 0]);
    constexpr auto dateFont = platform::fonts::fontId(platform::fonts::Font::RobotoL);
    const platform::ui::Rect dateBounds{bounds.x, static_cast<int16_t>(bounds.y + 40), bounds.width,
                                        gridLineHeight(canvas, dateFont)};
    canvas.text(dateBounds, date,
                {.font = dateFont, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::White});
    constexpr auto font = platform::fonts::fontId(platform::fonts::Font::NDot120);
    // Round line boxes up so font metrics do not introduce off-grid spacing.
    const platform::ui::Rect timeBounds{bounds.x, static_cast<int16_t>(dateBounds.y + dateBounds.height + 8),
                                        bounds.width, gridLineHeight(canvas, font)};
    canvas.text(timeBounds, time,
                {.font = font, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::White});
    _renderCalendar(canvas, bounds, props);
    if (!props.showUnlockHint && !props.charging) return;
    char charging[32] = "Charging";
    if (props.showUnlockHint) {
        snprintf(charging, sizeof(charging), "Swipe up to unlock");
    } else if (props.batteryKnown && props.percent >= 100) {
        snprintf(charging, sizeof(charging), "Fully Charged");
    } else if (props.batteryKnown) {
        snprintf(charging, sizeof(charging), "Charged %u%%", static_cast<unsigned>(props.percent));
    }
    constexpr auto chargingFont = platform::fonts::fontId(platform::fonts::Font::RobotoM);
    // Keep layout spacing on the four-pixel grid independently of font glyph insets.
    const platform::ui::Rect chargingBounds{bounds.x, static_cast<int16_t>(timeBounds.y + timeBounds.height + 16),
                                            bounds.width, gridLineHeight(canvas, chargingFont)};
    canvas.text(
        chargingBounds, charging,
        {.font = chargingFont, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::LightGray});
}

void LockPage::_renderCalendar(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds,
                               const Props& props) const {
    using namespace freeink::ui;
    using platform::fonts::Font;
    using platform::fonts::fontId;
    const size_t count = std::min(size_t{3}, props.events.size());
    if (count == 0) return;
    constexpr int16_t sideInset = 32, lineGap = 4, itemGap = 4, groupGap = 16, groupBottom = 4;
    constexpr int16_t calendarBottomInset = 24;
    const int16_t titleHeight = gridLineHeight(canvas, fontId(Font::RobotoL));
    const int16_t detailHeight = gridLineHeight(canvas, fontId(Font::RobotoM));
    const int16_t labelHeight = gridLineHeight(canvas, fontId(Font::RobotoS));
    const int16_t itemHeight = titleHeight + lineGap + detailHeight;
    const auto events = props.events.first(count);
    int16_t contentHeight = 0;
    for (size_t i = 0; i < count; ++i) {
        const bool newGroup = i == 0 || events[i].isTomorrow != events[i - 1].isTomorrow;
        contentHeight += newGroup ? labelHeight + groupBottom + (i == 0 ? 0 : groupGap) : itemGap;
        contentHeight += itemHeight;
    }
    int16_t y = bounds.y + bounds.height - calendarBottomInset - contentHeight;
    for (size_t i = 0; i < count; ++i) {
        const auto& event = events[i];
        const bool newGroup = i == 0 || event.isTomorrow != events[i - 1].isTomorrow;
        if (newGroup) {
            if (i != 0) y += groupGap;
            canvas.text({static_cast<int16_t>(bounds.x + sideInset), y,
                         static_cast<int16_t>(bounds.width - 2 * sideInset), labelHeight},
                        event.isTomorrow ? "Tomorrow" : "Upcoming",
                        {.font = fontId(Font::RobotoS), .color = Color::LightGray});
            y += labelHeight + groupBottom;
        } else {
            y += itemGap;
        }
        const int16_t rowHeight = itemHeight;
        calendarRow(canvas,
                    {static_cast<int16_t>(bounds.x + sideInset), y, static_cast<int16_t>(bounds.width - 2 * sideInset),
                     rowHeight},
                    event, titleHeight, detailHeight);
        y += rowHeight;
    }
}

}  // namespace apps::shell::pages
