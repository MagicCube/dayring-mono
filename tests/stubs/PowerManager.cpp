#include "platform/hal/PowerManager.h"

#include "platform/hal/Hardware.h"

namespace platform::hal {
PowerManager& powerManager() {
    static PowerManager instance;
    return instance;
}
}  // namespace platform::hal
