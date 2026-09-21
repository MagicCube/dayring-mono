#pragma once

namespace platform::hal {

// Called only from the application loop after the display finishes refreshing.
void restartDevice();

}  // namespace platform::hal
