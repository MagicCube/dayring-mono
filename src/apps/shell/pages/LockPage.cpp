#include "LockPage.h"

#include <cstdio>

#include "../../../platform/fonts/Fonts.h"

namespace apps::shell::pages {

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
                                        canvas.lineHeight(dateFont)};
    canvas.text(dateBounds, date,
                {.font = dateFont, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::White});
    constexpr auto font = platform::fonts::fontId(platform::fonts::Font::NDot120);
    // Keep the established clock position while accounting for font line metrics.
    const platform::ui::Rect timeBounds{bounds.x, static_cast<int16_t>(dateBounds.y + dateBounds.height + 7),
                                        bounds.width, canvas.lineHeight(font)};
    canvas.text(timeBounds, time,
                {.font = font, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::White});
    if (props.showUnlockHint) {
        constexpr auto hintFont = platform::fonts::fontId(platform::fonts::Font::RobotoM);
        const platform::ui::Rect hintBounds{
            bounds.x, static_cast<int16_t>(bounds.y + bounds.height - 40 - canvas.lineHeight(hintFont)), bounds.width,
            canvas.lineHeight(hintFont)};
        canvas.text(
            hintBounds, "Swipe Up to Unlock",
            {.font = hintFont, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::LightGray});
    }
    if (!props.charging) return;
    char charging[24] = "Charging";
    if (props.batteryKnown && props.percent >= 100) {
        snprintf(charging, sizeof(charging), "Fully Charged");
    } else if (props.batteryKnown) {
        snprintf(charging, sizeof(charging), "Charged %u%%", static_cast<unsigned>(props.percent));
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
