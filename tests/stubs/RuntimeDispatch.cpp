#include "platform/runtime/Input.h"
#include "platform/ui/ApplicationContainer.h"

namespace platform::runtime {
class ApplicationManager;

// Facade policy tests do not run a frame or require hardware-backed dispatch.
void dispatchInput(Shell&) {
}

void renderFrame(ui::ApplicationContainer&) {
}
}  // namespace platform::runtime

unsigned long testNowMs = 0;

unsigned long millis() {
    return testNowMs;
}
namespace platform::hal {
bool powerButtonPressed() {
    return false;
}
bool hasInputActivity() {
    return false;
}
Rtc::DateTime clockTime() {
    return {};
}
bool readBatteryPercent(uint8_t&) {
    return false;
}
bool readCharging(bool&) {
    return false;
}
}  // namespace platform::hal
