#pragma once

#include <cstdint>

namespace platform::runtime {

struct InputEvent {
    enum class Type {
        PowerPress,
        TouchPress,
        TouchMove,
        TouchRelease,
        Confirm,
        Back,
        Previous,
        Next,
        FocusPrevious,
        FocusNext,
        Swipe
    };
    Type type;
    // Touch coordinates use the application's logical display orientation.
    int16_t x = 0;
    int16_t y = 0;
    // Swipe carries its origin here and its endpoint in x/y.
    int16_t startX = 0;
    int16_t startY = 0;
};

}  // namespace platform::runtime
