#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace platform::runtime {
// Query keys and values are percent-decoded; '+' remains a literal plus.
// Duplicate keys use the first value. Unknown keys remain available to callers.
struct LocationQuery {
    std::vector<std::pair<std::string, std::string>> parameters;
    [[nodiscard]] static std::optional<LocationQuery> parse(std::string_view location);
    [[nodiscard]] std::optional<std::string_view> value(std::string_view key) const;
};

struct AppURL {
    std::string applicationName;
    // Application-local path and optional query/fragment, starting with a slash.
    std::string location;

    // Preserve path, query, and fragment text without percent-decoding or parsing parameters.
    [[nodiscard]] static std::optional<AppURL> parse(std::string_view url);
    [[nodiscard]] static bool isValidApplicationName(std::string_view name);
};
}  // namespace platform::runtime
