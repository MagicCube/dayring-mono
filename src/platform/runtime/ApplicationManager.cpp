#include "ApplicationManager.h"

#include <algorithm>
#include <utility>

#include "../ui/PageController.h"
#include "AppURL.h"
#include "TransitionGuard.h"

namespace platform::runtime {

ApplicationManager::ApplicationManager(std::size_t transientLimit)
    : _transientLimit(std::max<std::size_t>(1, transientLimit)) {
}

ApplicationManager::~ApplicationManager() {
    TransitionGuard guard(_transitioning);
    _leaveCurrent();
}

bool ApplicationManager::registerApplication(std::string_view name, Factory factory, Residency residency) {
    if (_transitioning || !factory || name == "home" || !AppURL::isValidApplicationName(name)) return false;
    if (_find(name) != _kNoApplication) return false;
    _entries.push_back({std::string(name), factory, residency, nullptr});
    return true;
}

ApplicationManager::ApplicationId ApplicationManager::_find(std::string_view name) const {
    for (ApplicationId id = 0; id < _entries.size(); ++id) {
        if (_entries[id].name == name) return id;
    }
    return _kNoApplication;
}

bool ApplicationManager::open(std::string_view url, OpenMode mode) {
    if (_transitioning || _interruption || (_active() && _active()->navigation()._transitioning)) return false;
    const auto error = checkRoute(url);
    if (error != RouteError::None && !(mode == OpenMode::Default && error == RouteError::UnknownPage)) return false;
    auto parsed = AppURL::parse(url);
    if (!parsed) return false;
    const auto id = _find(parsed->applicationName);
    TransitionGuard guard(_transitioning);
    const auto location = parsed->location;
    if (!_enter(id, Intent{std::move(*parsed)})) return false;
    return mode != OpenMode::Exact || (_active()->navigation().currentLocation() == location &&
                                       _active()->navigation().currentPageController() != nullptr);
}

std::optional<std::vector<RouteDescription>> ApplicationManager::describeRoutes() {
    if (_transitioning || _active()) return std::nullopt;
    TransitionGuard guard(_transitioning);
    std::vector<RouteDescription> result;
    for (ApplicationId id = 0; id < _entries.size(); ++id) {
        if (!_prepare(id)) return std::nullopt;
        for (auto& page : _entries[id].instance->router().descriptions()) {
            result.push_back({_entries[id].name, std::move(page)});
        }
    }
    return result;
}

RouteError ApplicationManager::checkRoute(std::string_view url) {
    if (_transitioning || (_active() && _active()->navigation()._transitioning)) return RouteError::Unavailable;
    const auto parsed = AppURL::parse(url);
    if (!parsed) return RouteError::InvalidURL;
    const auto id = _find(parsed->applicationName);
    if (id == _kNoApplication) return RouteError::UnknownApplication;
    TransitionGuard guard(_transitioning);
    if (!_prepare(id)) return RouteError::Unavailable;
    const auto path = parsed->location.substr(0, parsed->location.find_first_of("?#"));
    const auto* page = _entries[id].instance->router().resolve(path.c_str());
    if (!page) return RouteError::UnknownPage;
    if (!LocationQuery::parse(parsed->location) || !page->acceptsLocation(parsed->location))
        return RouteError::InvalidParameters;
    return RouteError::None;
}

std::string ApplicationManager::currentURL() const {
    const auto* active = _active();
    if (!active || !active->navigation().currentPageController()) return {};
    return "app://" + _entries[_activeId].name + std::string(active->navigation().currentLocation());
}

bool ApplicationManager::_prepare(ApplicationId id) {
    if (id >= _entries.size() || !_entries[id].factory) return false;
    if (_entries[id].instance) return true;
    auto instance = _entries[id].factory();
    if (!instance) return false;
    instance->_owner = this;
    instance->onCreate();
    if (!instance->router().resolve("/")) return false;
    _entries[id].instance = std::move(instance);
    return true;
}

bool ApplicationManager::_enter(ApplicationId id, const Intent& intent) {
    // Prepare before eviction so a failed open preserves the foreground and cache.
    // A newly created instance can briefly exceed the retained-instance limit.
    if (!_prepare(id)) return false;
    if (id != _activeId) _leaveCurrent();
    _activeId = id;
    _retainTransient(id);
    _entries[id].instance->_activate(intent);
    return true;
}

void ApplicationManager::leave() {
    if (_transitioning || _interruption || (_active() && _active()->navigation()._transitioning)) return;
    TransitionGuard guard(_transitioning);
    _leaveCurrent();
}

void ApplicationManager::_leaveCurrent() {
    auto* application = _active();
    if (!application) return;
    application->_deactivate();
    _activeId = _kNoApplication;
}

void ApplicationManager::_retainTransient(ApplicationId id) {
    if (_entries[id].residency != Residency::Transient) return;
    std::erase(_recentTransients, id);
    _recentTransients.push_back(id);
    if (_recentTransients.size() <= _transientLimit) return;
    const auto oldest = _recentTransients.front();
    _recentTransients.erase(_recentTransients.begin());
    // The evicted app already received onLeave() when it lost the foreground.
    _entries[oldest].instance.reset();
}

bool ApplicationManager::_interrupt(const Intent& intent) {
    if (_transitioning || _interruption || !_active() || !_active()->navigation()._entered ||
        _active()->navigation()._transitioning)
        return false;
    TransitionGuard guard(_transitioning);
    const auto target = _find(intent.url.applicationName);
    if (target == _kNoApplication || _entries[target].residency != Residency::Resident || !_prepare(target))
        return false;
    auto& navigation = _entries[target].instance->navigation();
    const auto path = intent.url.location.substr(0, intent.url.location.find_first_of("?#"));
    auto* page = _entries[target].instance->router().resolve(path.c_str());
    if (!page || navigation._temporary || !LocationQuery::parse(intent.url.location) ||
        !page->acceptsLocation(intent.url.location))
        return false;
    const Interruption interruption{_activeId, target};
    ApplicationNavigation::Entry temporary{intent.url.location, page};
    _leaveCurrent();
    navigation._temporary = std::move(temporary);
    _interruption = interruption;
    _activeId = target;
    _entries[target].instance->_activate(Intent{intent.url, Intent::Reason::Present});
    return true;
}

bool ApplicationManager::_restore() {
    if (_transitioning || !_interruption || _activeId != _interruption->presented) return false;
    TransitionGuard guard(_transitioning);
    const auto previous = _interruption->previous;
    _leaveCurrent();
    _entries[_interruption->presented].instance->navigation()._temporary.reset();
    _activeId = previous;
    auto& entry = _entries[previous];
    const Intent intent{{entry.name, std::string(entry.instance->navigation().currentLocation())},
                        Intent::Reason::Restore};
    entry.instance->_activate(intent);
    _interruption.reset();
    return true;
}

Application* ApplicationManager::_active() const {
    return _activeId == _kNoApplication ? nullptr : _entries[_activeId].instance.get();
}

void ApplicationManager::update() {
    if (auto* application = _active()) application->update();
}

bool ApplicationManager::onInput(const InputEvent& event) {
    auto* application = _active();
    return application && application->onInput(event);
}

bool ApplicationManager::needsRender() const {
    auto* application = _active();
    return application && application->needsRender();
}

bool ApplicationManager::allowsIdleLock() const {
    auto* application = _active();
    return application && application->allowsIdleLock();
}

ui::PageController* ApplicationManager::currentPageController() const {
    const auto* application = _active();
    return application ? application->navigation().currentPageController() : nullptr;
}

bool ApplicationManager::render(ui::Canvas& canvas, const ui::Rect& bounds, bool force) {
    auto* application = _active();
    if (!application || (!force && !application->needsRender())) return false;
    application->_needsRender = false;
    application->render(canvas, bounds);
    return true;
}

}  // namespace platform::runtime
