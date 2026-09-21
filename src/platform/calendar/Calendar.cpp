#include "Calendar.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <set>

#ifdef ARDUINO
#include <cJSON.h>
#else
#include "../../../third_party/cjson/cJSON.h"
#endif

namespace platform::calendar {
namespace {

using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

bool boundedJson(std::string_view text) {
    int depth = 0;
    size_t tokens = 0;
    bool quoted = false, escape = false;
    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == 0) return false;
        if (quoted) {
            if (c < 0x20 || (!escape && c == '\\' && text.substr(i, 6) == "\\u0000")) return false;
            if (escape)
                escape = false;
            else if (c == '\\')
                escape = true;
            else if (c == '"')
                quoted = false;
        } else if (c == '"')
            quoted = true;
        else if (c == ',' || c == ':') {
            if (++tokens > maxEvents * 14 + 32) return false;
        } else if (c == '{' || c == '[') {
            if (++depth > 6) return false;
        } else if (c == '}' || c == ']')
            --depth;
    }
    return depth == 0 && !quoted;
}

bool validUtf8(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        uint32_t code = static_cast<unsigned char>(text[i++]);
        if (code < 0x80) continue;
        unsigned count = 0;
        uint32_t minimum = 0;
        if (code >= 0xC2 && code <= 0xDF) {
            count = 1;
            minimum = 0x80;
            code &= 0x1F;
        } else if (code >= 0xE0 && code <= 0xEF) {
            count = 2;
            minimum = 0x800;
            code &= 0x0F;
        } else if (code >= 0xF0 && code <= 0xF4) {
            count = 3;
            minimum = 0x10000;
            code &= 0x07;
        } else
            return false;
        if (i + count > text.size()) return false;
        while (count--) {
            const auto byte = static_cast<unsigned char>(text[i++]);
            if ((byte & 0xC0) != 0x80) return false;
            code = (code << 6) | (byte & 0x3F);
        }
        if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
    }
    return true;
}

Json decode(std::string_view text, size_t limit) {
    if (text.empty() || text.size() > limit || !boundedJson(text) || !validUtf8(text)) return {nullptr, cJSON_Delete};
    const char* end = nullptr;
    Json result(cJSON_ParseWithLengthOpts(text.data(), text.size(), &end, false), cJSON_Delete);
    if (!result) return result;
    while (end < text.data() + text.size() && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) ++end;
    if (end != text.data() + text.size()) result.reset();
    return result;
}

bool fields(const cJSON* object, std::initializer_list<std::string_view> names) {
    if (!cJSON_IsObject(object)) return false;
    std::set<std::string_view> seen;
    for (auto* item = object->child; item; item = item->next) {
        if (!item->string || !seen.insert(item->string).second ||
            std::find(names.begin(), names.end(), item->string) == names.end())
            return false;
    }
    return seen.size() == names.size();
}

const cJSON* property(const cJSON* value, const char* key) {
    return cJSON_GetObjectItemCaseSensitive(value, key);
}

std::optional<std::string> string(const cJSON* value, const char* key, size_t maximum) {
    const auto* item = property(value, key);
    if (!cJSON_IsString(item) || !item->valuestring || std::strlen(item->valuestring) > maximum) return std::nullopt;
    return item->valuestring;
}

std::optional<DateTime> timestamp(const cJSON* value, const char* key) {
    const auto text = string(value, key, 32);
    return text ? parseDateTime(*text) : std::nullopt;
}

bool integer(const cJSON* value, const char* key, size_t minimum, size_t maximum, size_t& output) {
    const auto* item = property(value, key);
    if (!cJSON_IsNumber(item) || item->valuedouble < minimum || item->valuedouble > maximum) return false;
    output = static_cast<size_t>(item->valuedouble);
    return item->valuedouble == output;
}

std::optional<Event> event(const cJSON* value) {
    if (!fields(value, {"instanceId", "title", "location", "start", "end", "isAllDay"})) return std::nullopt;
    auto id = string(value, "instanceId", 128), title = string(value, "title", 2048),
         location = string(value, "location", 2048);
    auto start = timestamp(value, "start"), end = timestamp(value, "end");
    const auto* allDay = property(value, "isAllDay");
    if (!id || id->empty() || !title || !location || !start || !end || end->utcSeconds < start->utcSeconds ||
        !cJSON_IsBool(allDay))
        return std::nullopt;
    const bool isAllDay = cJSON_IsTrue(allDay);
    if (isAllDay &&
        (start->localSeconds() % 86400 || end->localSeconds() % 86400 || end->localSeconds() <= start->localSeconds()))
        return std::nullopt;
    return Event{std::move(*id), std::move(*title), std::move(*location), std::move(*start), std::move(*end), isAllDay};
}

}  // namespace

int64_t DateTime::localSeconds() const {
    return utcSeconds + offsetSeconds;
}

std::optional<DateTime> parseDateTime(std::string_view value) {
    if (value.size() != 20 && value.size() != 25) return std::nullopt;
    if (value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' || value[16] != ':')
        return std::nullopt;
    auto number = [&](size_t at, size_t count) {
        int n = 0;
        for (size_t i = at; i < at + count; ++i) {
            if (value[i] < '0' || value[i] > '9') return -1;
            n = n * 10 + value[i] - '0';
        }
        return n;
    };
    const int y = number(0, 4), m = number(5, 2), d = number(8, 2);
    const int h = number(11, 2), minute = number(14, 2), second = number(17, 2);
    using namespace std::chrono;
    const year_month_day day{year{y}, month{static_cast<unsigned>(m)}, std::chrono::day{static_cast<unsigned>(d)}};
    if (y < 2000 || y > 2099 || !day.ok() || h < 0 || h > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
        return std::nullopt;
    int offset = 0;
    if (value.size() == 20) {
        if (value[19] != 'Z') return std::nullopt;
    } else {
        const int oh = number(20, 2), om = number(23, 2);
        if ((value[19] != '+' && value[19] != '-') || value[22] != ':' || oh < 0 || om < 0 || om > 59 || oh > 14 ||
            (oh == 14 && om != 0))
            return std::nullopt;
        offset = (oh * 3600 + om * 60) * (value[19] == '-' ? -1 : 1);
    }
    const auto local =
        duration_cast<seconds>(sys_days{day}.time_since_epoch()).count() + h * 3600 + minute * 60 + second;
    return DateTime{std::string(value), local - offset, offset};
}

std::optional<Snapshot> parseSnapshot(std::string_view json) {
    auto root = decode(json, maxSnapshotBytes);
    if (!root || !fields(root.get(),
                         {"schemaVersion", "generatedAt", "timeZone", "windowStart", "windowEndExclusive", "events"}))
        return std::nullopt;
    size_t version = 0;
    auto zone = string(root.get(), "timeZone", 64);
    auto generated = timestamp(root.get(), "generatedAt"), start = timestamp(root.get(), "windowStart"),
         end = timestamp(root.get(), "windowEndExclusive");
    const auto* events = property(root.get(), "events");
    if (!integer(root.get(), "schemaVersion", 1, 1, version) || !zone || zone->empty() || !generated || !start ||
        !end || !cJSON_IsArray(events) || cJSON_GetArraySize(events) > static_cast<int>(maxEvents) ||
        start->localSeconds() % 86400 || end->localSeconds() - start->localSeconds() != 2 * 86400 ||
        generated->localSeconds() < start->localSeconds() ||
        generated->localSeconds() >= start->localSeconds() + 86400 || generated->utcSeconds < start->utcSeconds ||
        generated->utcSeconds >= end->utcSeconds)
        return std::nullopt;
    Snapshot result{std::move(*zone), std::move(*generated), std::move(*start), std::move(*end), {}};
    std::set<std::string> ids;
    for (auto* item = events->child; item; item = item->next) {
        auto parsed = event(item);
        if (!parsed || !ids.insert(parsed->instanceId).second ||
            parsed->start.utcSeconds >= result.windowEndExclusive.utcSeconds ||
            (parsed->end.utcSeconds > parsed->start.utcSeconds
                 ? parsed->end.utcSeconds <= result.windowStart.utcSeconds
                 : parsed->start.utcSeconds < result.windowStart.utcSeconds))
            return std::nullopt;
        result.events.push_back(std::move(*parsed));
    }
    std::sort(result.events.begin(), result.events.end(), [](const Event& a, const Event& b) {
        return a.start.utcSeconds == b.start.utcSeconds ? a.instanceId < b.instanceId
                                                        : a.start.utcSeconds < b.start.utcSeconds;
    });
    return result;
}

std::optional<Manifest> parseManifest(std::string_view json) {
    auto root = decode(json, 512);
    if (!root || !fields(root.get(), {"snapshotId", "byteLength", "chunkBytes"})) return std::nullopt;
    auto id = string(root.get(), "snapshotId", 64);
    Manifest result;
    if (!id || id->empty() ||
        !std::all_of(id->begin(), id->end(),
                     [](unsigned char c) {
                         return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
                     }) ||
        !integer(root.get(), "byteLength", 1, maxSnapshotBytes, result.byteLength) ||
        !integer(root.get(), "chunkBytes", 1, 10240, result.chunkBytes))
        return std::nullopt;
    result.snapshotId = std::move(*id);
    return result;
}

std::string readRequest(const Manifest& manifest, size_t offset) {
    return "{\"snapshotId\":\"" + manifest.snapshotId + "\",\"offset\":" + std::to_string(offset) + "}";
}

}  // namespace platform::calendar
