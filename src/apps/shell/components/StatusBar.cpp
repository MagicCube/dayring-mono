#include "StatusBar.h"

#include <FreeInkUILayout.h>
#include <components/bars/battery-indicator.h>

#include <cstdio>

#include "../../../platform/fonts/Fonts.h"
#include "../../../platform/hal/Hardware.h"

namespace apps::shell::components {
void StatusBar::reset() {
    _clock.reset();
    _sampled = false;
}

bool StatusBar::update() {
    bool changed = _clock.update();
    const uint32_t now = millis();
    if (!_sampled || now - _chargingSampledAt >= 1000U) {
        bool charging = _charging;
        if (platform::hal::readCharging(charging) && charging != _charging) {
            _charging = charging;
            changed = true;
        }
        _chargingSampledAt = now;
    }
    if (!_sampled || now - _batterySampledAt >= 180000U) {
        uint8_t percent = _percent;
        if (platform::hal::readBatteryPercent(percent)) {
            changed = changed || !_batteryKnown || percent != _percent;
            _percent = percent;
            _batteryKnown = true;
        }
        _batterySampledAt = now;
    }
    _sampled = true;
    return changed;
}

void StatusBar::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    using namespace freeink::ui;
    canvas.fill(bounds, Paint::solid(Color::White));
    const Rect content = bounds.inset(Insets{.top = 0, .right = 12, .bottom = 0, .left = 12});
    Rect slots[3]{};
    // Empty center content measures zero; both side slots share the remaining width.
    layoutLinear(
        content, Axis::Row, 0, 3,
        [](uint8_t index) { return index == 1 ? LayoutLength::fixed(0) : LayoutLength::flexible(); },
        [&](uint8_t index, Rect rect) { slots[index] = rect; });
    char time[6];
    snprintf(time, sizeof(time), "%02u:%02u", static_cast<unsigned>(_clock.time().hour),
             static_cast<unsigned>(_clock.time().minute));
    canvas.text(slots[0], time,
                {.font = platform::fonts::fontId(platform::fonts::Font::RobotoS), .color = Color::Black});
    const DeviceContext device{.width = bounds.right(), .height = bounds.bottom()};
    const InputSnapshot input{};
    InteractionBuffer<1> interactions;
    Frame<1> frame(canvas, device, input, interactions);
    batteryIndicator(frame, slots[2],
                     {.percent = _percent, .charging = _charging, .glyphWidth = 35, .glyphHeight = 20});
}
}  // namespace apps::shell::components
