#include "ApplicationRouter.h"

#include <algorithm>
#include <string_view>

#include "../ui/PageController.h"

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

bool ApplicationRouter::registerPage(const char* path, ui::PageController& page, std::string_view help) {
    if (!validPath(path) || resolve(path) || (page._owner && page._owner != &_application)) return false;
    _entries.push_back({std::string(path), &page, std::string(help)});
    page._owner = &_application;
    return true;
}

std::vector<ApplicationRouter::Description> ApplicationRouter::descriptions() const {
    std::vector<Description> result;
    for (const auto& entry : _entries) {
        result.push_back({entry.path, entry.page->isFullscreen(), entry.help});
    }
    return result;
}

ui::PageController* ApplicationRouter::resolve(const char* path) const {
    if (!validPath(path)) return nullptr;
    const auto match =
        std::find_if(_entries.begin(), _entries.end(), [path](const Entry& entry) { return entry.path == path; });
    return match == _entries.end() ? nullptr : match->page;
}

}  // namespace platform::runtime
