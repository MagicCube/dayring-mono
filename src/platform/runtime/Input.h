#pragma once

namespace platform::runtime {

class Shell;
// Call once after each HAL update, including while the display is refreshing.
void dispatchInput(Shell& shell);

}  // namespace platform::runtime
