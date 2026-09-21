#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace platform::ui {

// Value-owned QR matrix. Supports versions 1–10 with medium or stronger error correction.
class QRCode {
   public:
    [[nodiscard]] bool encode(std::string_view text);
    [[nodiscard]] int size() const;
    [[nodiscard]] bool module(int x, int y) const;

   private:
    std::array<uint8_t, 408> _data{};
};

}  // namespace platform::ui
