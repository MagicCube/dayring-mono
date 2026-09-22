#pragma once

#include <deque>
#include <string_view>

#include "platform/rpc/MessageChannel.h"

namespace preview {

bool configureWeather(std::string_view value);
bool hasWeatherFixture();

class WeatherPeer {
   public:
    void start();
    void update(uint32_t now);
    bool send(std::span<const uint8_t> bytes);
    bool receive(platform::rpc::Packet& packet);

   private:
    platform::rpc::MessageChannel _channel;
    std::deque<platform::rpc::Packet> _inbound;
    uint32_t _now = 0;
};

}  // namespace preview
