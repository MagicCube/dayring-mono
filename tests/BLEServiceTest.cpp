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
    assert(service.session() == 0);
    test::event(BLE_GAP_EVENT_SUBSCRIBE);
    const auto session = service.session();
    assert(session != 0);
    const std::array<uint8_t, 8> frame{0xD1, 0, 1, 0, 1, 0, 0, 0};
    assert(service.send(session, frame) && test::notification.size() == 8);
    os_mbuf input{std::string(reinterpret_cast<const char*>(frame.data()), frame.size())};
    ble_gatt_access_ctxt access{&input};
    assert(test::rpcWrite.access_cb(1, 0, &access, test::rpcWrite.arg) == 0);
    platform::rpc::Packet received;
    assert(service.receive(received) && received.session == session && received.size == 8);
    assert(service.packetSize() == 20);
    const std::array<uint8_t, 244> largeFrame{0xD2};
    assert(!service.send(session, largeFrame));
    test::attMTU = 247;
    test::event(BLE_GAP_EVENT_MTU);
    assert(service.packetSize() == 244 && service.send(session, largeFrame));
    input.value.assign(reinterpret_cast<const char*>(largeFrame.data()), largeFrame.size());
    assert(test::rpcWrite.access_cb(1, 0, &access, test::rpcWrite.arg) == 0);
    assert(service.receive(received) && received.size == 244);
    input.value.resize(245);
    assert(test::rpcWrite.access_cb(1, 0, &access, test::rpcWrite.arg) == BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN);
    input.value.assign(reinterpret_cast<const char*>(frame.data()), frame.size());
    test::attMTU = 23;
    test::event(BLE_GAP_EVENT_MTU);
    assert(test::event(BLE_GAP_EVENT_REPEAT_PAIRING) == BLE_GAP_REPEAT_PAIRING_IGNORE);
    test::event(BLE_GAP_EVENT_ENC_CHANGE, 1);
    assert(service.state() == State::Failed && test::disconnects == 1);
    assert(service.session() == 0 && !service.send(session, frame));
    test::event(BLE_GAP_EVENT_DISCONNECT);
    assert(service.state() == State::Advertising && test::advertisements == 2);
    // An out-of-range disconnect preserves bonds and permits encrypted reconnection.
    test::event(BLE_GAP_EVENT_CONNECT);
    test::event(BLE_GAP_EVENT_ENC_CHANGE);
    assert(service.state() == State::Secured);
    test::failAdvertising = true;
    test::event(BLE_GAP_EVENT_DISCONNECT);
    assert(service.state() == State::Failed);
    const int attempts = test::advertisements;
    service.update(2000);
    assert(test::advertisements == attempts + 1);
    service.update(2001);
    assert(test::advertisements == attempts + 1);
    test::failAdvertising = false;
    service.update(3000);
    assert(service.state() == State::Advertising && test::advertisements == attempts + 2);
    test::event(BLE_GAP_EVENT_ADV_COMPLETE);
    assert(service.state() == State::Advertising && test::advertisements == attempts + 3);
    test::event(BLE_GAP_EVENT_CONNECT, 1);
    assert(service.state() == State::Advertising);
    test::event(BLE_GAP_EVENT_CONNECT);
    test::event(BLE_GAP_EVENT_ENC_CHANGE);
    assert(service.state() == State::Secured && service.bondCount() == 1);
    service.stop();
    const int stoppedAttempts = test::advertisements;
    service.update(5000);
    assert(test::advertisements == stoppedAttempts);
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
    // Bonded reconnect callbacks can arrive in any order, including CONNECT last.
    const std::array<std::array<int, 3>, 6> orders{{
        {BLE_GAP_EVENT_CONNECT, BLE_GAP_EVENT_ENC_CHANGE, BLE_GAP_EVENT_SUBSCRIBE},
        {BLE_GAP_EVENT_CONNECT, BLE_GAP_EVENT_SUBSCRIBE, BLE_GAP_EVENT_ENC_CHANGE},
        {BLE_GAP_EVENT_ENC_CHANGE, BLE_GAP_EVENT_CONNECT, BLE_GAP_EVENT_SUBSCRIBE},
        {BLE_GAP_EVENT_ENC_CHANGE, BLE_GAP_EVENT_SUBSCRIBE, BLE_GAP_EVENT_CONNECT},
        {BLE_GAP_EVENT_SUBSCRIBE, BLE_GAP_EVENT_CONNECT, BLE_GAP_EVENT_ENC_CHANGE},
        {BLE_GAP_EVENT_SUBSCRIBE, BLE_GAP_EVENT_ENC_CHANGE, BLE_GAP_EVENT_CONNECT},
    }};
    uint32_t previousSession = 0;
    for (const auto& order : orders) {
        test::event(BLE_GAP_EVENT_DISCONNECT);
        assert(service.session() == 0);
        uint32_t readySession = 0;
        for (const auto event : order) {
            test::event(event);
            if (readySession) assert(service.session() == readySession);
            readySession = service.session();
        }
        assert(service.state() == State::Secured && readySession && readySession != previousSession);
        previousSession = readySession;
        assert(test::rpcWrite.access_cb(1, 0, &access, test::rpcWrite.arg) == 0);
        const auto requests = test::securityRequests;
        test::event(BLE_GAP_EVENT_CONNECT);
        assert(test::securityRequests == requests && service.session() == readySession);
        assert(service.receive(received) && received.session == readySession);
    }
    ble_hs_cfg.reset_cb(1);
    assert(service.session() == 0 && service.state() == State::Failed);
    ble_hs_cfg.sync_cb();
    test::event(BLE_GAP_EVENT_ENC_CHANGE);
    test::event(BLE_GAP_EVENT_SUBSCRIBE);
    test::event(BLE_GAP_EVENT_CONNECT);
    assert(service.state() == State::Secured && service.session() != previousSession);
    const auto finalSession = service.session();
    test::failClear = true;
    assert(service.clearBonds());
    assert(service.bondResetState() == BLEService::BondResetState::Failed && service.bondCount() == 1);
    service.stop();
    test::failClear = false;
    assert(service.start());
    test::event(BLE_GAP_EVENT_CONNECT);
    test::event(BLE_GAP_EVENT_ENC_CHANGE);
    test::event(BLE_GAP_EVENT_SUBSCRIBE);
    const auto resetSession = service.session();
    assert(resetSession && resetSession != finalSession);
    assert(service.clearBonds());
    assert(service.bondResetState() == BLEService::BondResetState::Complete);
    assert(service.bondCount() == 0 && test::bonds.empty() && test::cccds == 0);
    assert(service.session() == resetSession && service.send(resetSession, frame));
    assert(!service.clearBonds());
    const auto beforeDisconnect = test::advertisements;
    test::cccds = 1;
    test::event(BLE_GAP_EVENT_DISCONNECT);
    assert(test::cccds == 0 && test::advertisements == beforeDisconnect);
    std::cout << "BLE lifecycle and pairing policy tests passed (simulated NimBLE API)\n";
}
