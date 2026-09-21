#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace platform::calendar {

inline constexpr size_t maxSnapshotBytes = 64 * 1024;
inline constexpr size_t maxEvents = 256;

struct DateTime {
    std::string text;
    int64_t utcSeconds = 0;
    int32_t offsetSeconds = 0;
    [[nodiscard]] int64_t localSeconds() const;
    bool operator==(const DateTime&) const = default;
};

struct Event {
    std::string instanceId;
    std::string title;
    std::string location;
    DateTime start;
    DateTime end;
    bool isAllDay = false;
    bool operator==(const Event&) const = default;
};

struct Snapshot {
    std::string timeZone;
    DateTime generatedAt;
    DateTime windowStart;
    DateTime windowEndExclusive;
    std::vector<Event> events;
};

struct Manifest {
    std::string snapshotId;
    size_t byteLength = 0;
    size_t chunkBytes = 0;
};

[[nodiscard]] std::optional<DateTime> parseDateTime(std::string_view value);
[[nodiscard]] std::optional<Snapshot> parseSnapshot(std::string_view json);
[[nodiscard]] std::optional<Manifest> parseManifest(std::string_view json);
[[nodiscard]] std::string readRequest(const Manifest& manifest, size_t offset);

}  // namespace platform::calendar
