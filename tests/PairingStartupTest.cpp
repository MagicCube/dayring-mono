#include <cassert>
#include <iostream>

#include "apps/shell/ShellApplication.h"
#include "apps/shell/components/StatusBarController.h"
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

uint32_t BLEService::session() const {
    return 0;
}

size_t BLEService::packetSize() const {
    return 20;
}

void BLEService::disconnect() {
}

bool BLEService::clearBonds() {
    return false;
}

BLEService::BondResetState BLEService::bondResetState() const {
    return BondResetState::Idle;
}

bool BLEService::receive(rpc::Packet&) {
    return false;
}

bool BLEService::send(uint32_t, std::span<const uint8_t>) {
    return false;
}

bool BLEService::start() {
    return true;
}

void BLEService::update(uint32_t) {
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
    apps::shell::components::StatusBarController statusBar;
    (void)statusBar.update();
    assert(!statusBar.update());
    radioState = platform::ble::BLEService::State::Connected;
    assert(statusBar.update());
    testNowMs = 999;
    assert(!statusBar.update());
    testNowMs = 1000;
    assert(statusBar.update());
    assert(!statusBar.update());
    testNowMs = 2000;
    assert(statusBar.update());
    radioState = platform::ble::BLEService::State::Secured;
    assert(statusBar.update());
    testNowMs = 3000;
    assert(!statusBar.update());
    radioState = platform::ble::BLEService::State::Advertising;
    assert(statusBar.update());
    testNowMs = 4000;
    assert(!statusBar.update());
    statusBar.setTheme(platform::ui::Theme::Dark);
    assert(statusBar.update());
    assert(!statusBar.update());
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
