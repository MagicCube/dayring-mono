#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace platform::ui {

// Square row-major binary matrix; bit (size - 1) is the leftmost dot.
struct DotMatrix {
    static constexpr uint8_t maxSize = 16;
    uint8_t size = 11;
    std::array<uint16_t, maxSize> rows{};

    [[nodiscard]] constexpr bool isValid() const {
        return size > 0 && size <= maxSize;
    }

    [[nodiscard]] constexpr bool isSet(size_t column, size_t row) const {
        return isValid() && column < size && row < size && (rows[row] & (1u << (size - 1 - column))) != 0;
    }
};

}  // namespace platform::ui
