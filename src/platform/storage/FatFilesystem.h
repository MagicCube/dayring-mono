#pragma once

#ifdef ARDUINO
#include <FFat.h>

namespace platform::storage {

inline bool mountFatFilesystem() {
    // Shared by font startup and calendar/weather persistence; never format on failure.
    static bool mounted = false;
    if (!mounted) mounted = FFat.begin(false, "/ffat", 4, "storage");
    return mounted;
}

}  // namespace platform::storage
#endif
