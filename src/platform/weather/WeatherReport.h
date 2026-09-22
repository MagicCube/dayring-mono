#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace platform::weather {

inline constexpr uint16_t weatherGetMethod = 12;

struct WeatherReport {
    uint16_t weatherCode;
    int16_t minTempC;
    int16_t maxTempC;
    std::string city;
    bool operator==(const WeatherReport&) const = default;
};

[[nodiscard]] std::optional<WeatherReport> decodeWeatherReport(std::span<const uint8_t> bytes);

}  // namespace platform::weather
