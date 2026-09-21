#include <iostream>

#include "NimBLEStub.h"
#include "platform/ble/BLEProfile.h"
#include "platform/ble/services/BLEService.h"

int main() {
    using platform::ble::BLEService;
    using State = BLEService::State;
    namespace test = ble_test;
    ble_addr_t owner{};
    owner.val[0] = 42;
    test::bonds.push_back(owner);
    BLEService service;
    assert(service.start());
    assert(service.start() && test::starts == 1);
    assert(service.state() == State::Advertising && service.bondCount() == 1);
    assert(ble_hs_cfg.sm_bonding == 1 && ble_hs_cfg.sm_sc == 1 && ble_hs_cfg.sm_mitm == 0);
    assert(test::characteristic.flags & BLE_GATT_CHR_F_READ_ENC);
    assert(test::characteristic.min_key_size == 16);
    assert(test::uuids[0] == platform::ble::serviceUUID);
    assert(test::uuids[1] == platform::ble::pairingStatusUUID);

    test::event(BLE_GAP_EVENT_CONNECT);
    assert(service.state() == State::Connected && test::securityRequests == 1);
    assert(test::readStatus().first == BLE_ATT_ERR_INSUFFICIENT_ENC);
    test::connection.sec_state.encrypted = true;
    assert(test::readStatus().second == "pending");
    test::connection.sec_state.bonded = true;
    // Another device's saved bond must never acknowledge this peer.
    assert(test::readStatus().second == "pending");
    test::connection.peer_id_addr = owner;
    test::event(BLE_GAP_EVENT_ENC_CHANGE);
    assert(service.state() == State::Secured);
    assert(test::readStatus().second == platform::ble::pairedResponse);
    assert(test::event(BLE_GAP_EVENT_REPEAT_PAIRING) == BLE_GAP_REPEAT_PAIRING_IGNORE);
    test::event(BLE_GAP_EVENT_ENC_CHANGE, 1);
    assert(service.state() == State::Failed && test::disconnects == 1);
    test::event(BLE_GAP_EVENT_DISCONNECT);
    assert(service.state() == State::Advertising && test::advertisements == 2);
    service.stop();
    service.stop();
    assert(!service.isRunning() && test::stops == 1 && test::deinits == 1);
    assert(test::bonds.size() == 1);
    assert(service.start() && service.state() == State::Advertising);
    service.stop();

    test::failAdvertising = true;
    assert(!service.start());
    assert(!service.isRunning() && service.state() == State::Failed);
    assert(!test::hostRunning && test::deinits == 3);
    test::failAdvertising = false;
    test::failRegistration = true;
    assert(!service.start());
    assert(!test::hostRunning && test::deinits == 4 && test::stops == 3);
    test::failRegistration = false;
    assert(service.start());
    std::cout << "BLE lifecycle and pairing policy tests passed (simulated NimBLE API)\n";
}
