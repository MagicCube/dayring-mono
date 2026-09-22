#include "HostBLE.h"

#include "HostWeather.h"
#include "platform/ble/services/BLEService.h"

namespace {

platform::ble::BLEService::State bluetoothState = platform::ble::BLEService::State::Secured;

}  // namespace

namespace preview {

bool configureBluetooth(std::string_view state) {
    using State = platform::ble::BLEService::State;
    if (state == "connected")
        bluetoothState = State::Secured;
    else if (state == "connecting")
        bluetoothState = State::Connected;
    else if (state == "disconnected")
        bluetoothState = State::Advertising;
    else
        return false;
    return true;
}

}  // namespace preview

namespace platform::ble {

// Replace only the radio boundary; production controllers consume the configured state.
class BLEService::Backend {
   public:
    bool running = false;
    preview::WeatherPeer weather;
};

BLEService::BLEService() : _backend(std::make_unique<Backend>()) {
}

BLEService::~BLEService() = default;

bool BLEService::start() {
    _backend->running = true;
    _backend->weather.start();
    return true;
}

void BLEService::stop() {
    _backend->running = false;
}

void BLEService::update(uint32_t now) {
    _backend->weather.update(now);
}

bool BLEService::isRunning() const {
    return _backend->running;
}

BLEService::State BLEService::state() const {
    return isRunning() ? bluetoothState : State::Stopped;
}

std::size_t BLEService::bondCount() const {
    return 0;
}

uint32_t BLEService::session() const {
    return _backend->running && preview::hasWeatherFixture() ? 1 : 0;
}

size_t BLEService::packetSize() const {
    return 244;
}

void BLEService::disconnect() {
}

bool BLEService::clearBonds() {
    return false;
}

BLEService::BondResetState BLEService::bondResetState() const {
    return BondResetState::Idle;
}

bool BLEService::receive(rpc::Packet& packet) {
    return _backend->weather.receive(packet);
}

bool BLEService::send(uint32_t, std::span<const uint8_t> bytes) {
    return _backend->weather.send(bytes);
}

}  // namespace platform::ble
