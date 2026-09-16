#include "ApplicationNavigation.h"

#include <utility>

#include "../ui/Page.h"
#include "Application.h"
#include "TransitionGuard.h"

namespace platform::runtime {
ApplicationNavigation::ApplicationNavigation(Application& application) : _application(application) {
}

bool ApplicationNavigation::push(const char* location) {
    return _navigate(location, false);
}

bool ApplicationNavigation::replace(const char* location) {
    return _navigate(location, true);
}

bool ApplicationNavigation::_navigate(const char* location, bool replaceTop) {
    if (!_active || _transitioning || _temporary || !location) return false;
    const std::string_view text(location);
    if (!text.starts_with('/') || text.starts_with("//")) return false;
    const std::string path(text.substr(0, text.find_first_of("?#")));
    auto* page = _application.router().resolve(path.c_str());
    if (!page) return false;
    Entry next{std::string(text), page};
    TransitionGuard guard(_transitioning);
    _leavePage();
    if (replaceTop && !_entries.empty()) {
        _entries.back() = std::move(next);
    } else {
        _entries.push_back(std::move(next));
    }
    _enterPage();
    return true;
}

bool ApplicationNavigation::pop() {
    if (!_active || _transitioning || _temporary || !canPop()) return false;
    TransitionGuard guard(_transitioning);
    _leavePage();
    _entries.pop_back();
    _enterPage();
    return true;
}

bool ApplicationNavigation::canPop() const {
    return !_temporary && _entries.size() > 1;
}

ui::Page* ApplicationNavigation::currentPage() const {
    if (_temporary) return _temporary->page;
    return _entries.empty() ? nullptr : _entries.back().page;
}

std::string_view ApplicationNavigation::currentLocation() const {
    if (_temporary) return _temporary->location;
    return _entries.empty() ? std::string_view{} : _entries.back().location;
}

void ApplicationNavigation::_leavePage() {
    if (!_entered) return;
    _entered = false;

    currentPage()->onLeave();
}

void ApplicationNavigation::_enterPage() {
    if (_entered || !currentPage()) return;
    _entered = true;

    currentPage()->onEnter(currentLocation());
    _application.requestRender();
}

void ApplicationNavigation::_resume() {
    TransitionGuard guard(_transitioning);
    _enterPage();
}

void ApplicationNavigation::_suspend() {
    _active = false;
    TransitionGuard guard(_transitioning);
    _leavePage();
}
}  // namespace platform::runtime
