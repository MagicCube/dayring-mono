#include <cassert>
#include <iostream>

#include "apps/shell/ShellApplication.h"
#include "platform/runtime/Shell.h"

extern unsigned long testNowMs;

namespace {

std::size_t bonds = 0;
platform::ble::BLEService::State radioState = platform::ble::BLEService::State::Advertising;

}  // namespace

namespace platform::hal {

void testFrontlightBrightness(uint8_t) {
}

}  // namespace platform::hal

// Replace the radio at the link boundary; exercise the real Shell and page controllers.
namespace platform::ble {

class BLEService::Backend {};

BLEService::BLEService() = default;
BLEService::~BLEService() = default;

bool BLEService::start() {
    return true;
}

void BLEService::stop() {
}

bool BLEService::isRunning() const {
    return true;
}

BLEService::State BLEService::state() const {
    return radioState;
}

std::size_t BLEService::bondCount() const {
    return bonds;
}

}  // namespace platform::ble

int main() {
    auto& shell = platform::runtime::Shell::instance();
    assert(shell.registerApplication(
        "shell",
        []() -> std::unique_ptr<platform::runtime::Application> {
            return std::make_unique<apps::shell::ShellApplication>();
        },
        platform::runtime::Residency::Resident));
    assert(shell.startServices());
    assert(shell.openStartupPage());
    assert(shell.applicationManager().currentURL() == "app://shell/pairing");
    assert(!shell.applicationManager().allowsIdleLock());
    assert(!shell.goHome() && !shell.lock() && !shell.open("app://shell/lock"));
    testNowMs = 120000;
    shell.update();
    assert(!shell.isLocked() && shell.applicationManager().currentURL() == "app://shell/pairing");
    assert(shell.onInput({platform::runtime::InputEvent::Type::PowerPress}));
    assert(!shell.isLocked());
    radioState = platform::ble::BLEService::State::Connected;
    shell.update();
    assert(shell.applicationManager().currentURL() == "app://shell/pairing");
    radioState = platform::ble::BLEService::State::Secured;
    shell.update();
    assert(!shell.isHome());
    bonds = 1;
    shell.update();
    assert(shell.isHome() && !shell.isLocked());
    // A stored bond permits startup while the phone is offline.
    radioState = platform::ble::BLEService::State::Advertising;
    assert(shell.openStartupPage() && shell.isHome());
    assert(shell.lock() && shell.isLocked());
    assert(shell.unlock());
    // Direct route entry must enforce the same lock prohibition as first boot.
    assert(shell.open("app://shell/pairing?source=preview"));
    assert(!shell.lock() && !shell.open("app://shell/lock"));
    assert(shell.onInput({platform::runtime::InputEvent::Type::PowerPress}));
    testNowMs += 120000;
    shell.update();
    assert(!shell.isLocked());
    assert(shell.applicationManager().currentURL() == "app://shell/pairing?source=preview");
    bonds = 0;
    assert(shell.openStartupPage());
    assert(shell.prepareFirmwareUpdate());
    assert(shell.applicationManager().currentURL() == "app://shell/firmware-update");
    std::cout << "Pairing startup, completion, offline restart and update escape passed\n";
}
