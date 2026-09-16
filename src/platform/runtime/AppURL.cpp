#include "AppURL.h"

#include <algorithm>
#include <utility>

namespace platform::runtime {
bool AppURL::isValidApplicationName(std::string_view name) {
    return !name.empty() && std::all_of(name.begin(), name.end(), [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '_' || character == '-';
    });
}

std::optional<AppURL> AppURL::parse(std::string_view url) {
    constexpr std::string_view scheme = "app://";
    if (!url.starts_with(scheme)) return std::nullopt;
    url.remove_prefix(scheme.size());
    const auto name = url.substr(0, url.find_first_of("/?#"));
    if (!isValidApplicationName(name)) return std::nullopt;
    const auto suffix = url.substr(name.size());
    std::string location(suffix);
    if (location.empty() || location.front() == '?' || location.front() == '#') location.insert(0, "/");
    return AppURL{std::string(name), std::move(location)};
}
}  // namespace platform::runtime
