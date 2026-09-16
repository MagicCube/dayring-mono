#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace platform::runtime {
struct AppURL {
    std::string applicationName;
    // Application-local path and optional query/fragment, starting with a slash.
    std::string location;

    // Preserve path, query, and fragment text without percent-decoding or parsing parameters.
    [[nodiscard]] static std::optional<AppURL> parse(std::string_view url);
    [[nodiscard]] static bool isValidApplicationName(std::string_view name);
};
}  // namespace platform::runtime
