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
    _applicationManager.update();
    _applicationContainer.update();
    renderFrame(_applicationContainer);
}

bool Shell::open(std::string_view url) {
    auto parsed = AppURL::parse(url);
    if (!parsed) return false;
    if (parsed->applicationName == "home") {
        if (parsed->location != "/") return false;
        parsed = AppURL::parse(DAYRING_HOME_URL);
        if (!parsed || parsed->applicationName == "home") return false;
    }
    const auto path = parsed->location.substr(0, parsed->location.find_first_of("?#"));
    if (parsed->applicationName == "shell" && path == "/lock") return _lock(Intent{std::move(*parsed)});
    if (isLocked()) return false;
    return _applicationManager.open("app://" + parsed->applicationName + parsed->location);
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

bool Shell::_lock(const Intent& intent) {
    if (isLocked()) return true;
    if (!_applicationManager._interrupt(intent)) return false;
    _powerManager.setLocked(true);
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
