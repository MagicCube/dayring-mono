#include "LockScreen.h"

#include <Arduino.h>

#include <cstdio>

#include "../../../platform/fonts/Fonts.h"
#include "../../../platform/hal/RtcClock.h"

namespace apps::shell::pages {
void LockScreen::onEnter(std::string_view) {
    _clock.reset();
    (void)_clock.update();
}

bool LockScreen::update() {
    return _clock.update();
}

void LockScreen::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    char time[6];
    snprintf(time, sizeof(time), "%02u:%02u", static_cast<unsigned>(_clock.time().hour),
             static_cast<unsigned>(_clock.time().minute));
    canvas.fill(bounds, freeink::ui::Paint::solid(freeink::ui::Color::Black));
    constexpr auto font = platform::fonts::fontId(platform::fonts::Font::NDot120);
    const platform::ui::Rect timeBounds{bounds.x, static_cast<int16_t>(bounds.y + bounds.height / 8), bounds.width,
                                        canvas.lineHeight(font)};
    canvas.text(timeBounds, time,
                {.font = font, .align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::White});
}
}  // namespace apps::shell::pages
