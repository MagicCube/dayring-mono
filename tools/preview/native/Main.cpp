#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

#include "HostHardware.h"
#include "apps/RegisterApplications.h"
#include "platform/runtime/Shell.h"

namespace {
using platform::runtime::OpenMode;
using platform::runtime::RouteError;
using platform::runtime::Shell;

std::string jsonString(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (unsigned char ch : value) {
        if (ch == '"' || ch == '\\') {
            result += '\\';
            result += static_cast<char>(ch);
        } else if (ch < 32) {
            result += "\\u00";
            result += hex[ch >> 4];
            result += hex[ch & 15];
        } else {
            result += static_cast<char>(ch);
        }
    }
    return result + '"';
}

int fail(int status, std::string_view code, std::string_view message) {
    std::cout << "{\"error\":{\"code\":" << jsonString(code) << ",\"message\":" << jsonString(message) << "}}\n";
    return status;
}

int listRoutes(Shell& shell) {
    const auto routes = shell.applicationManager().describeRoutes();
    if (!routes) return fail(5, "route_discovery_failed", "Unable to initialize registered applications.");
    std::cout << "{\"protocol\":1,\"routes\":[";
    bool first = true;
    for (const auto& route : *routes) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"application\":" << jsonString(route.application)
                  << ",\"url\":" << jsonString("app://" + route.application + route.page.path)
                  << ",\"fullscreen\":" << (route.page.fullscreen ? "true" : "false")
                  << ",\"help\":" << jsonString(route.page.help) << '}';
    }
    const auto home = Shell::resolveURL("app://home");
    std::cout << "],\"aliases\":{\"app://home\":";
    if (home)
        std::cout << jsonString("app://" + home->applicationName + home->location);
    else
        std::cout << "null";
    std::cout << "}}\n";
    return 0;
}

int routeFailure(RouteError error) {
    switch (error) {
        case RouteError::InvalidURL:
            return fail(2, "invalid_url", "Expected app://application/path?parameters.");
        case RouteError::UnknownApplication:
            return fail(3, "unknown_application", "Application not registered. Run: tools/preview/preview routes");
        case RouteError::UnknownPage:
            return fail(3, "unknown_page", "Page not registered. Run: tools/preview/preview routes");
        case RouteError::InvalidParameters:
            return fail(2, "invalid_parameters",
                        "Invalid query encoding or page parameters. Run: tools/preview/preview help APP");
        default:
            return fail(5, "route_unavailable", "Application could not be initialized.");
    }
}

bool number(std::string_view value, int maximum, uint8_t& out) {
    unsigned int parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed > unsigned(maximum))
        return false;
    out = static_cast<uint8_t>(parsed);
    return true;
}

int capture(Shell& shell, char** argv) {
    uint8_t hour, minute, battery, charging;
    if (!number(argv[3], 23, hour) || !number(argv[4], 59, minute) || !number(argv[5], 100, battery) ||
        !number(argv[6], 1, charging))
        return fail(2, "invalid_state", "Invalid hour, minute, battery, or charging state.");
    preview::configureHardware(hour, minute, battery, charging != 0);
    const auto target = Shell::resolveURL(argv[2]);
    if (!target) return routeFailure(RouteError::InvalidURL);
    const auto resolved = "app://" + target->applicationName + target->location;
    const auto error = shell.applicationManager().checkRoute(resolved);
    if (error != RouteError::None) return routeFailure(error);
    if (!shell.open("app://home", OpenMode::Exact) || !shell.open(resolved, OpenMode::Exact))
        return fail(5, "open_failed", "Exact navigation failed; no screenshot was produced.");
    if (shell.applicationManager().currentURL() != resolved)
        return fail(5, "route_mismatch", "The application redirected away from the requested page.");
    shell.update();
    if (!preview::hasFrame()) return fail(5, "missing_frame", "The render cycle submitted no frame.");
    const auto pixels = preview::portraitPixels();
    std::cout << "{\"protocol\":1,\"width\":480,\"height\":800,\"format\":\"gray8\",\"resolved_url\":"
              << jsonString(resolved) << "}\n";
    std::cout.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    return std::cout ? 0 : 5;
}
}  // namespace

int main(int argc, char** argv) {
    auto& shell = Shell::instance();
    if (!apps::registerApplications(shell)) return fail(5, "registration_failed", "Application registration failed.");
    if (argc == 2 && std::string_view(argv[1]) == "routes") return listRoutes(shell);
    if (argc == 7 && std::string_view(argv[1]) == "capture") return capture(shell, argv);
    return fail(2, "invalid_arguments", "Use the tools/preview/preview launcher. The native protocol is internal.");
}
