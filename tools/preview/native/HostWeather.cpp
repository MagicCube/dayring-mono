#include "HostWeather.h"

#include <algorithm>

namespace preview {
namespace {

bool sampleWeather = false;

}

bool configureWeather(std::string_view value) {
    if (value != "empty" && value != "sample") return false;
    sampleWeather = value == "sample";
    return true;
}

bool hasWeatherFixture() {
    return sampleWeather;
}

void WeatherPeer::start() {
    _channel.reset();
    _inbound.clear();
    if (sampleWeather) (void)_channel.enqueue({.id = 60000, .method = 0, .payload = {2}}, _now);
    update(_now);
}

void WeatherPeer::update(uint32_t now) {
    _now = now;
    _channel.pump(now, 244, [this](auto bytes) {
        platform::rpc::Packet packet;
        packet.session = 1;
        packet.size = bytes.size();
        std::copy(bytes.begin(), bytes.end(), packet.bytes.begin());
        _inbound.push_back(packet);
        return true;
    });
}

bool WeatherPeer::send(std::span<const uint8_t> bytes) {
    using namespace platform::rpc;
    if (auto message = _channel.receive(bytes, _now); message && message->kind == Kind::Request) {
        const bool weather = message->method == 12;
        std::vector<uint8_t> payload =
            weather ? std::vector<uint8_t>{1, 113, 0, 16, 0, 22, 0, 8, 'S', 'h', 'a', 'n', 'g', 'h', 'a', 'i'}
                    : std::vector<uint8_t>{7};
        (void)_channel.enqueue({.kind = weather ? Kind::Response : Kind::Error,
                                .id = message->id,
                                .method = message->method,
                                .payload = std::move(payload)},
                               _now);
    }
    update(_now);
    return true;
}

bool WeatherPeer::receive(platform::rpc::Packet& packet) {
    if (_inbound.empty()) return false;
    packet = _inbound.front();
    _inbound.pop_front();
    return true;
}

}  // namespace preview
