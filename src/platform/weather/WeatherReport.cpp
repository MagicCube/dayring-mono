#include "WeatherReport.h"

#include <algorithm>

namespace platform::weather {
namespace {

int16_t temperature(std::span<const uint8_t> bytes, size_t offset) {
    const auto raw = uint16_t(bytes[offset]) | (uint16_t(bytes[offset + 1]) << 8);
    return static_cast<int16_t>(raw <= 32767 ? raw : int32_t(raw) - 65536);
}

bool validCity(std::span<const uint8_t> bytes) {
    // Reject malformed UTF-8, control characters, overlong encodings and surrogates.
    for (size_t i = 0; i < bytes.size();) {
        const auto first = bytes[i++];
        if (first < 0x80) {
            if (first < 0x20 || first == 0x7f) return false;
            continue;
        }
        const size_t count = first >= 0xc2 && first <= 0xdf   ? 1
                             : first <= 0xef && first >= 0xe0 ? 2
                             : first >= 0xf0 && first <= 0xf4 ? 3
                                                              : 0;
        if (!count || i + count > bytes.size()) return false;
        uint32_t point = first & (0x7f >> (count + 1));
        for (size_t j = 0; j < count; ++j) {
            if ((bytes[i] & 0xc0) != 0x80) return false;
            point = (point << 6) | (bytes[i++] & 0x3f);
        }
        if (point < (count == 1   ? 0x80U
                     : count == 2 ? 0x800U
                                  : 0x10000U) ||
            point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff) || (point >= 0x80 && point <= 0x9f))
            return false;
    }
    return std::any_of(bytes.begin(), bytes.end(), [](auto c) { return c != ' '; });
}

}  // namespace

std::optional<WeatherReport> decodeWeatherReport(std::span<const uint8_t> bytes) {
    if (bytes.size() < 9 || bytes[0] != 1 || bytes[7] == 0 || bytes[7] > 128 || bytes.size() != 8U + bytes[7] ||
        !validCity(bytes.subspan(8)))
        return std::nullopt;
    const auto code = static_cast<uint16_t>(uint16_t(bytes[1]) | (uint16_t(bytes[2]) << 8));
    const auto low = temperature(bytes, 3), high = temperature(bytes, 5);
    if (!code || low < -100 || high > 100 || low > high) return std::nullopt;
    return WeatherReport{code, low, high, std::string(bytes.begin() + 8, bytes.end())};
}

}  // namespace platform::weather
