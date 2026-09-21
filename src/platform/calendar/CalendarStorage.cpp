#include "CalendarStorage.h"

#include <cstdint>
#include <limits>

#include "Calendar.h"

namespace platform::calendar {
namespace {

constexpr std::string_view magic = "DRCAL001";
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

bool valid(std::string_view bytes) {
    return bytes.size() > headerSize && bytes.size() <= maxSnapshotBytes + headerSize && bytes.starts_with(magic) &&
           get(bytes, 8, 8) > 0 && get(bytes, 16, 4) == bytes.size() - headerSize &&
           get(bytes, 20, 4) == checksum(bytes) && parseSnapshot(bytes.substr(headerSize)).has_value();
}

}  // namespace

FileCalendarStorage::FileCalendarStorage(std::unique_ptr<CalendarFiles> files) : _files(std::move(files)) {
}

std::optional<std::string> FileCalendarStorage::load() {
    _generation = 0;
    _slot = 1;
    std::optional<std::string> result;
    for (unsigned slot = 0; slot < 2; ++slot) {
        auto bytes = _files->read(slot);
        if (!bytes || !valid(*bytes)) continue;
        const auto generation = get(*bytes, 8, 8);
        if (generation <= _generation) continue;
        _generation = generation;
        _slot = slot;
        result = bytes->substr(headerSize);
    }
    return result;
}

bool FileCalendarStorage::save(std::string_view json) {
    if (!parseSnapshot(json)) return false;
    // Re-discover the valid slot if storage became available after an earlier mount/read failure.
    load();
    if (_generation == std::numeric_limits<uint64_t>::max()) return false;
    std::string bytes(headerSize, '\0');
    bytes.replace(0, magic.size(), magic);
    put(bytes, 8, _generation + 1, 8);
    put(bytes, 16, json.size(), 4);
    bytes.append(json);
    put(bytes, 20, checksum(bytes), 4);
    const unsigned target = 1 - _slot;
    if (!_files->write(target, bytes)) return false;
    const auto readback = _files->read(target);
    if (!readback || *readback != bytes) return false;
    _slot = target;
    ++_generation;
    return true;
}

}  // namespace platform::calendar
