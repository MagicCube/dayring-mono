#include <cassert>
#include <deque>
#include <iostream>
#include <limits>

#include "platform/hal/services/DeviceControlService.h"

using namespace platform;

namespace {

uint32_t nowMs = 0;
bool displayIdle = true;
int restarts = 0, clears = 0;
ble::BLEService::BondResetState resetState = ble::BLEService::BondResetState::Idle;

}  // namespace

unsigned long millis() {
    return nowMs;
}

namespace platform::hal {

bool displayReady() {
    return displayIdle;
}

void restartDevice() {
    ++restarts;
}

}  // namespace platform::hal

namespace platform::ble {

class BLEService::Backend {};

BLEService::BLEService() = default;
BLEService::~BLEService() = default;

bool BLEService::start() {
    return true;
}

void BLEService::stop() {
}

void BLEService::update(uint32_t) {
}

bool BLEService::isRunning() const {
    return true;
}

BLEService::State BLEService::state() const {
    return State::Secured;
}

size_t BLEService::bondCount() const {
    return 1;
}

uint32_t BLEService::session() const {
    return 1;
}

size_t BLEService::packetSize() const {
    return 20;
}

void BLEService::disconnect() {
}

bool BLEService::receive(rpc::Packet&) {
    return false;
}

bool BLEService::send(uint32_t, std::span<const uint8_t>) {
    return false;
}

bool BLEService::clearBonds() {
    ++clears;
    resetState = BondResetState::Pending;
    return true;
}

BLEService::BondResetState BLEService::bondResetState() const {
    return resetState;
}

}  // namespace platform::ble

namespace {

class Transport final : public rpc::Transport {
   public:
    uint32_t generation = 1;
    std::deque<rpc::Packet> incoming;
    rpc::Packet last;

    uint32_t session() const override {
        return generation;
    }

    bool receive(rpc::Packet& packet) override {
        if (incoming.empty()) return false;
        packet = incoming.front();
        incoming.pop_front();
        return true;
    }

    bool send(uint32_t, std::span<const uint8_t> bytes) override {
        last.size = bytes.size();
        std::copy(bytes.begin(), bytes.end(), last.bytes.begin());
        return true;
    }
};

}  // namespace

int main() {
    Transport transport;
    tasking::TaskDispatchService tasks;
    rpc::RPCService rpc(transport, tasks);
    ble::BLEService ble;
    hal::DeviceControlService control(rpc, ble);
    assert(tasks.start() && rpc.start() && control.start() && control.start());
    uint16_t nextID = 1;
    auto call = [&](uint16_t method, std::vector<uint8_t> payload = {}) {
        const auto id = nextID++;
        auto packet = rpc::encode({.id = id, .method = method, .payload = std::move(payload)});
        packet.session = transport.generation;
        transport.incoming.push_back(packet);
        nowMs += 10;
        tasks.update(nowMs);
        auto result = rpc::decode({transport.last.bytes.data(), transport.last.size});
        assert(result && result->id == id);
        return *result;
    };
    assert(call(rpc::helloMethod, {2}).kind == rpc::Kind::Response);
    assert(call(rpc::deviceRebootMethod, {1}).payload == std::vector<uint8_t>{2});
    assert(call(rpc::pairingResetMethod, {2}).payload == std::vector<uint8_t>{2});
    assert(call(rpc::pairingResetMethod, {1}).payload == std::vector<uint8_t>{2});
    assert(clears == 0 && restarts == 0);
    assert(call(rpc::pairingResetMethod).payload == std::vector<uint8_t>{0});
    assert(clears == 1 && restarts == 0);
    assert(call(rpc::pairingResetMethod).payload == std::vector<uint8_t>{3});
    assert(call(rpc::pairingResetMethod, {1}).payload == std::vector<uint8_t>{0});
    resetState = ble::BLEService::BondResetState::Failed;
    control.update(nowMs);
    assert(call(rpc::pairingResetMethod, {1}).payload == std::vector<uint8_t>{7});
    assert(restarts == 0);
    assert(call(rpc::pairingResetMethod).payload == std::vector<uint8_t>{0} && clears == 2);
    resetState = ble::BLEService::BondResetState::Complete;
    control.update(nowMs);
    nowMs += 2000;
    control.update(nowMs);
    assert(restarts == 0);
    assert(call(rpc::pairingResetMethod, {1}).payload == std::vector<uint8_t>{1});
    control.update(nowMs);
    const auto accepted = nowMs;
    assert(call(rpc::deviceRebootMethod).payload == std::vector<uint8_t>{3});
    nowMs = accepted + 999;
    control.update(nowMs);
    assert(restarts == 0);
    displayIdle = false;
    nowMs = accepted + 1000;
    control.update(nowMs);
    assert(restarts == 0);
    displayIdle = true;
    control.update(nowMs);
    assert(restarts == 1);
    control.stop();
    assert(call(rpc::deviceRebootMethod).payload == std::vector<uint8_t>{1});
    assert(control.start());
    resetState = ble::BLEService::BondResetState::Idle;
    // The restart grace interval remains correct across the uint32 clock rollover.
    nowMs = std::numeric_limits<uint32_t>::max() - 510;
    assert(call(rpc::deviceRebootMethod).payload.empty());
    assert(restarts == 1);
    nowMs = 498;
    control.update(nowMs);
    assert(restarts == 1);
    nowMs = 499;
    control.update(nowMs);
    assert(restarts == 2 && clears == 2);
    control.stop();
    assert(control.start());
    assert(call(rpc::deviceRebootMethod).payload.empty());
    control.stop();
    nowMs += 2000;
    control.update(nowMs);
    assert(restarts == 2);
    // A reset still finishes if the requester leaves before polling its final result.
    assert(control.start());
    assert(call(rpc::pairingResetMethod).payload == std::vector<uint8_t>{0});
    resetState = ble::BLEService::BondResetState::Complete;
    transport.generation = 0;
    nowMs += 10;
    tasks.update(nowMs);
    control.update(nowMs);
    nowMs += 1000;
    control.update(nowMs);
    assert(restarts == 3);
    control.stop();
    assert(rpc.registerHandler(rpc::deviceRebootMethod, [](auto) { return rpc::Reply{}; }));
    assert(!control.start());
    assert(rpc.registerHandler(rpc::pairingResetMethod, [](auto) { return rpc::Reply{}; }));
    std::cout << "Device control RPC validation, reset failure/retry, deferred restart, display gating and lifecycle "
                 "passed\n";
}
