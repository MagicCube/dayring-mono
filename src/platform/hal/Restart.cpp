#include "Restart.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#endif

namespace platform::hal {

void restartDevice() {
#if defined(ARDUINO_ARCH_ESP32)
    esp_restart();
#endif
}

}  // namespace platform::hal
