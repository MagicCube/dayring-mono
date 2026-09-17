#include "Shell.h"

#include <utility>

#include "../hal/Hardware.h"
#include "AppURL.h"
#include "Display.h"
#include "Input.h"

#ifndef DAYRING_HOME_URL
#define DAYRING_HOME_URL "app://shell/"
#endif

namespace platform::runtime {

Shell::Shell() : _powerManager(hal::powerManager()) {
}

Shell& Shell::instance() {
    static Shell shell;
    return shell;
}

bool Shell::registerApplication(std::string_view name, ApplicationManager::Factory factory, Residency residency) {
    return _applicationManager.registerApplication(name, factory, residency);
}

void Shell::update() {
    dispatchInput(*this);
    if (_powerManager.isIdleLockDue() && _applicationManager.allowsIdleLock()) {
        (void)_lock(Intent{{"shell", "/lock"}}, hal::PowerManager::LockReason::Idle);
    }
    _applicationManager.update();
    _applicationContainer.update();
    renderFrame(_applicationContainer);
}

std::optional<AppURL> Shell::resolveURL(std::string_view url) {
    auto parsed = AppURL::parse(url);
    if (!parsed) return std::nullopt;
    if (parsed->applicationName == "home") {
        if (parsed->location != "/") return std::nullopt;
        parsed = AppURL::parse(DAYRING_HOME_URL);
        if (!parsed || parsed->applicationName == "home") return std::nullopt;
    }
    return parsed;
}

bool Shell::open(std::string_view url, OpenMode mode) {
    auto parsed = resolveURL(url);
    if (!parsed) return false;
    const auto resolved = "app://" + parsed->applicationName + parsed->location;
    if (mode == OpenMode::Exact && _applicationManager.checkRoute(resolved) != RouteError::None) return false;
    const auto path = parsed->location.substr(0, parsed->location.find_first_of("?#"));
    if (parsed->applicationName == "shell" && path == "/lock") {
        if (!_lock(Intent{std::move(*parsed)})) return false;
        return mode != OpenMode::Exact || _applicationManager.currentURL() == resolved;
    }
    if (isLocked()) return false;
    return _applicationManager.open(resolved, mode);
}

bool Shell::goHome() {
    return open("app://home");
}

bool Shell::isHome() const {
    if (isLocked()) return false;
    const auto home = AppURL::parse(DAYRING_HOME_URL);
    const auto* active = _applicationManager._active();
    return home && home->applicationName != "home" && active &&
           _applicationManager._activeId == _applicationManager._find(home->applicationName) &&
           active->navigation().currentLocation() == home->location;
}

void Shell::_handleHomeGesture() {
    if (isLocked() || isHome()) return;
    (void)goHome();
}

bool Shell::lock() {
    return _lock(Intent{{"shell", "/lock"}});
}

bool Shell::_lock(const Intent& intent, hal::PowerManager::LockReason reason) {
    if (isLocked()) return true;
    if (!_applicationManager._interrupt(intent)) return false;
    _powerManager.setLocked(true, reason);
    return true;
}

bool Shell::unlock() {
    if (!isLocked()) return true;
    if (!_applicationManager._restore()) return false;
    _powerManager.setLocked(false);
    return true;
}

bool Shell::isLocked() const {
    return _applicationManager._interruption.has_value();
}

ApplicationManager& Shell::applicationManager() {
    return _applicationManager;
}

const ApplicationManager& Shell::applicationManager() const {
    return _applicationManager;
}

bool Shell::onInput(const InputEvent& event) {
    if (event.type == InputEvent::Type::PowerPress) return isLocked() ? unlock() : lock();
    _powerManager.notifyActivity();
    return _applicationContainer.onInput(event);
}

}  // namespace platform::runtime
