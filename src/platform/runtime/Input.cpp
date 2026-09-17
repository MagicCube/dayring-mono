#include "Input.h"

#include "../hal/Hardware.h"
#include "Display.h"
#include "Shell.h"

namespace platform::runtime {

void dispatchInput(Shell& shell) {
    if (hal::powerButtonPressed()) {
        shell.onInput({InputEvent::Type::PowerPress});
        return;
    }
    float x = 0;
    float y = 0;
    float startX = 0;
    float startY = 0;
    if (hal::touchSwiped(startX, startY, x, y)) {
        const auto device = displayDevice();
        const auto start = freeink::ui::touchToLogical(device, startX, startY);
        const auto end = freeink::ui::touchToLogical(device, x, y);
        shell.onInput({InputEvent::Type::Swipe, end.x, end.y, start.x, start.y});
        return;
    }
    if (!hal::touchTapped(x, y)) return;
    const auto point = freeink::ui::touchToLogical(displayDevice(), x, y);
    shell.onInput({InputEvent::Type::TouchRelease, point.x, point.y});
}

}  // namespace platform::runtime
