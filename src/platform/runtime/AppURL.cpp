#include "AppURL.h"

#include <algorithm>
#include <utility>

namespace platform::runtime {
namespace {

int hexDigit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

std::optional<std::string> decodeQuery(std::string_view text) {
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char value = text[i];
        if (value == '%') {
            if (i + 2 >= text.size()) return std::nullopt;
            const int high = hexDigit(text[i + 1]);
            const int low = hexDigit(text[i + 2]);
            if (high < 0 || low < 0) return std::nullopt;
            value = static_cast<char>(high * 16 + low);
            i += 2;
        }
        if (value == '\0') return std::nullopt;
        result.push_back(value);
    }
    return result;
}

}  // namespace

std::optional<LocationQuery> LocationQuery::parse(std::string_view location) {
    LocationQuery result;
    location = location.substr(0, location.find('#'));
    const auto query = location.find('?');
    if (query == location.npos) return result;
    auto remaining = location.substr(query + 1);
    while (!remaining.empty()) {
        const auto end = remaining.find('&');
        const auto pair = remaining.substr(0, end);
        const auto equals = pair.find('=');
        auto key = decodeQuery(pair.substr(0, equals));
        auto value = decodeQuery(equals == pair.npos ? std::string_view{} : pair.substr(equals + 1));
        if (!key || !value) return std::nullopt;
        if (!pair.empty()) result.parameters.emplace_back(std::move(*key), std::move(*value));
        if (end == remaining.npos) break;
        remaining.remove_prefix(end + 1);
    }
    return result;
}

std::optional<std::string_view> LocationQuery::value(std::string_view key) const {
    for (const auto& [name, value] : parameters) {
        if (name == key) return value;
    }
    return std::nullopt;
}

bool AppURL::isValidApplicationName(std::string_view name) {
    return !name.empty() && std::all_of(name.begin(), name.end(), [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '_' || character == '-';
    });
}

std::optional<AppURL> AppURL::parse(std::string_view url) {
    if (std::any_of(url.begin(), url.end(), [](unsigned char ch) { return ch <= 32 || ch == 127; }))
        return std::nullopt;
    constexpr std::string_view scheme = "app://";
    if (!url.starts_with(scheme)) return std::nullopt;
    url.remove_prefix(scheme.size());
    const auto name = url.substr(0, url.find_first_of("/?#"));
    if (!isValidApplicationName(name)) return std::nullopt;
    const auto suffix = url.substr(name.size());
    std::string location(suffix);
    if (location.empty() || location.front() == '?' || location.front() == '#') location.insert(0, "/");
    if (location.starts_with("//")) return std::nullopt;
    return AppURL{std::string(name), std::move(location)};
}

}  // namespace platform::runtime
