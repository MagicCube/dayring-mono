#include "ApplicationRouter.h"

#include <algorithm>
#include <string_view>

#include "../ui/Page.h"

namespace platform::runtime {
namespace {
bool validPath(const char* path) {
    if (!path) return false;
    const std::string_view text(path);
    return text.starts_with('/') && !text.starts_with("//") && text.find_first_of("?#") == text.npos;
}
}  // namespace

ApplicationRouter::ApplicationRouter(Application& application) : _application(application) {
}

bool ApplicationRouter::registerPage(const char* path, ui::Page& page) {
    if (!validPath(path) || resolve(path) || (page._owner && page._owner != &_application)) return false;
    _entries.push_back({std::string(path), &page});
    page._owner = &_application;
    return true;
}

ui::Page* ApplicationRouter::resolve(const char* path) const {
    if (!validPath(path)) return nullptr;
    const auto match =
        std::find_if(_entries.begin(), _entries.end(), [path](const Entry& entry) { return entry.path == path; });
    return match == _entries.end() ? nullptr : match->page;
}
}  // namespace platform::runtime
