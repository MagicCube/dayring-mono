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
    constexpr auto font = platform::fonts::fontId(platform::fonts::Font::NDot120);
    constexpr auto statusFont = platform::fonts::fontId(platform::fonts::Font::RobotoM);
    freeink::ui::Stack<3> header(
        {bounds.x, static_cast<int16_t>(bounds.y + 40), bounds.width,
         static_cast<int16_t>(gridLineHeight(canvas, dateFont) + 8 + gridLineHeight(canvas, font) + 16 +
                              gridLineHeight(canvas, statusFont))},
        freeink::ui::Axis::Column, 8);
    header.fixed(gridLineHeight(canvas, dateFont));
    header.fixed(gridLineHeight(canvas, font));
    header.fixed(gridLineHeight(canvas, statusFont));
    header.layout();
    canvas.text(header.rect(0), date,
                {.font = dateFont, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::White});
    canvas.text(header.rect(1), time,
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
    const platform::ui::Rect chargingBounds = header.rect(2);
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

    constexpr int16_t sideInset = 32, itemGap = 4, groupGap = 16, groupBottom = 4;
    constexpr int16_t calendarBottomInset = 24;
    const int16_t titleHeight = gridLineHeight(canvas, fontId(Font::RobotoL));
    const int16_t detailHeight = gridLineHeight(canvas, fontId(Font::RobotoM));
    const int16_t labelHeight = gridLineHeight(canvas, fontId(Font::RobotoS));
    const int16_t itemHeight = titleHeight + 4 + detailHeight;
    const auto events = props.events.first(count);
    size_t firstTomorrow = count;
    for (size_t i = 0; i < count; ++i) {
        if (events[i].isTomorrow) {
            firstTomorrow = i;
            break;
        }
    }
    const auto groupHeight = [&](size_t begin, size_t end) {
        if (begin >= end) return int16_t{0};
        return static_cast<int16_t>(labelHeight + groupBottom + (end - begin) * itemHeight +
                                    (end - begin > 1 ? (end - begin - 1) * itemGap : 0));
    };
    const int16_t upcomingHeight = groupHeight(0, firstTomorrow);
    const int16_t tomorrowHeight = groupHeight(firstTomorrow, count);
    const int16_t totalHeight =
        static_cast<int16_t>(upcomingHeight + tomorrowHeight + (upcomingHeight && tomorrowHeight ? groupGap : 0));
    Stack<2> groups({static_cast<int16_t>(bounds.x + sideInset),
                     static_cast<int16_t>(bounds.y + bounds.height - calendarBottomInset - totalHeight),
                     static_cast<int16_t>(bounds.width - 2 * sideInset), totalHeight},
                    Axis::Column, groupGap);
    if (upcomingHeight) groups.fixed(upcomingHeight);
    if (tomorrowHeight) groups.fixed(tomorrowHeight);
    groups.layout();

    const auto renderGroup = [&](Rect groupBounds, size_t begin, size_t end, const char* label) {
        if (begin >= end) return;
        Stack<4> rows(groupBounds, Axis::Column, itemGap);
        rows.fixed(labelHeight);
        for (size_t i = begin; i < end; ++i) rows.fixed(itemHeight);
        rows.layout();
        canvas.text(rows.rect(0), label, {.font = fontId(Font::RobotoS), .color = Color::LightGray});
        for (size_t i = begin; i < end; ++i)
            calendarRow(canvas, rows.rect(static_cast<uint8_t>(i - begin + 1)), events[i], titleHeight, detailHeight);
    };

    uint8_t groupIndex = 0;
    if (upcomingHeight) renderGroup(groups.rect(groupIndex++), 0, firstTomorrow, "Upcoming");
    if (tomorrowHeight) renderGroup(groups.rect(groupIndex), firstTomorrow, count, "Tomorrow");
}

}  // namespace apps::shell::pages
