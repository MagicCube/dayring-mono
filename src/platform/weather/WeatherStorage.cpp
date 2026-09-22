#include "WeatherStorage.h"

#include <chrono>
#include <cstdint>
#include <limits>

namespace platform::weather {
namespace {

constexpr std::string_view magic = "DRWTH001";
constexpr size_t headerSize = 24;

uint32_t checksum(std::string_view bytes) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i >= 20 && i < headerSize) continue;
        crc ^= static_cast<uint8_t>(bytes[i]);
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

void put(std::string& bytes, size_t at, uint64_t value, size_t count) {
    for (size_t i = 0; i < count; ++i) bytes[at + i] = static_cast<char>(value >> (8 * i));
}

uint64_t get(std::string_view bytes, size_t at, size_t count) {
    uint64_t value = 0;
    for (size_t i = 0; i < count; ++i) value |= uint64_t(static_cast<uint8_t>(bytes[at + i])) << (8 * i);
    return value;
}

std::optional<CachedWeather> decode(std::string_view bytes) {
    if (bytes.size() <= headerSize || bytes.size() > 136 + headerSize || !bytes.starts_with(magic) ||
        !get(bytes, 8, 8) || get(bytes, 20, 4) != checksum(bytes))
        return std::nullopt;
    const auto date = static_cast<uint32_t>(get(bytes, 16, 4));
    if (!weatherDate(date / 10000, date / 100 % 100, date % 100)) return std::nullopt;
    const auto report =
        decodeWeatherReport({reinterpret_cast<const uint8_t*>(bytes.data() + headerSize), bytes.size() - headerSize});
    if (!report) return std::nullopt;
    return CachedWeather{date, *report};
}

}  // namespace

FileWeatherStorage::FileWeatherStorage(std::unique_ptr<WeatherFiles> files) : _files(std::move(files)) {
}

std::optional<CachedWeather> FileWeatherStorage::load() {
    _generation = 0;
    _slot = 1;
    std::optional<CachedWeather> result;
    for (unsigned slot = 0; slot < 2; ++slot) {
        auto bytes = _files->read(slot);
        const auto value = bytes ? decode(*bytes) : std::nullopt;
        if (!value) continue;
        const auto generation = get(*bytes, 8, 8);
        if (generation <= _generation) continue;
        _generation = generation;
        _slot = slot;
        result = value;
    }
    return result;
}

bool FileWeatherStorage::save(const CachedWeather& value) {
    if (!weatherDate(value.date / 10000, value.date / 100 % 100, value.date % 100)) return false;
    const auto& report = value.report;
    if (report.city.size() > 128) return false;
    std::string payload(8, '\0');
    payload[0] = 1;
    put(payload, 1, report.weatherCode, 2);
    put(payload, 3, static_cast<uint16_t>(report.minTempC), 2);
    put(payload, 5, static_cast<uint16_t>(report.maxTempC), 2);
    payload[7] = static_cast<char>(report.city.size());
    payload += report.city;
    if (!decodeWeatherReport({reinterpret_cast<const uint8_t*>(payload.data()), payload.size()})) return false;
    load();
    if (_generation == std::numeric_limits<uint64_t>::max()) return false;
    std::string bytes(headerSize, '\0');
    bytes.replace(0, magic.size(), magic);
    put(bytes, 8, _generation + 1, 8);
    put(bytes, 16, value.date, 4);
    bytes += payload;
    put(bytes, 20, checksum(bytes), 4);
    const unsigned target = 1 - _slot;
    if (!_files->write(target, bytes)) return false;
    const auto readback = _files->read(target);
    if (!readback || *readback != bytes) return false;
    _slot = target;
    ++_generation;
    return true;
}

std::optional<uint32_t> weatherDate(unsigned y, unsigned m, unsigned d) {
    using namespace std::chrono;
    if (y < 2000 || y > 2099 || m < 1 || m > 12 || d < 1 || d > 31 ||
        !year_month_day{year{static_cast<int>(y)}, month{m}, day{d}}.ok())
        return std::nullopt;
    return y * 10000 + m * 100 + d;
}

}  // namespace platform::weather
