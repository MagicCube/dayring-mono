#include "LockPage.h"

#include <cstdio>

#include "../../../platform/fonts/Fonts.h"

namespace apps::shell::pages {

void LockPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props) const {
    char time[6];
    snprintf(time, sizeof(time), "%02u:%02u", static_cast<unsigned>(props.hour), static_cast<unsigned>(props.minute));
    canvas.fill(bounds, freeink::ui::Paint::solid(freeink::ui::Color::Black));
    constexpr const char* weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    constexpr const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    char date[24];
    snprintf(date, sizeof(date), "%s, %s %u", weekdays[props.weekday % 7],
             months[props.month >= 1 && props.month <= 12 ? props.month - 1 : 0], static_cast<unsigned>(props.day));
    constexpr auto dateFont = platform::fonts::fontId(platform::fonts::Font::RobotoL);
    const platform::ui::Rect dateBounds{bounds.x, static_cast<int16_t>(bounds.y + 40), bounds.width,
                                        canvas.lineHeight(dateFont)};
    canvas.text(dateBounds, date,
                {.font = dateFont, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::White});
    constexpr auto font = platform::fonts::fontId(platform::fonts::Font::NDot120);
    // Account for glyph insets: 16 px of visible space below the date (including its comma).
    const platform::ui::Rect timeBounds{bounds.x, static_cast<int16_t>(dateBounds.y + dateBounds.height + 7),
                                        bounds.width, canvas.lineHeight(font)};
    canvas.text(timeBounds, time,
                {.font = font, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::White});
    if (!props.charging) return;
    char charging[24] = "CHARGING";
    if (props.batteryKnown) {
        snprintf(charging, sizeof(charging), "CHARGING %u%%", static_cast<unsigned>(props.percent));
    }
    constexpr auto chargingFont = platform::fonts::fontId(platform::fonts::Font::RobotoM);
    // Ndot's bottom inset and Roboto's top inset contribute 30 px of the 44 px visual gap.
    const platform::ui::Rect chargingBounds{bounds.x, static_cast<int16_t>(timeBounds.y + timeBounds.height + 14),
                                            bounds.width, canvas.lineHeight(chargingFont)};
    canvas.text(
        chargingBounds, charging,
        {.font = chargingFont, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::LightGray});
}

}  // namespace apps::shell::pages
