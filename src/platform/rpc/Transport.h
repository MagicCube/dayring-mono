#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace platform::rpc {

struct Packet {
    std::array<uint8_t, 20> bytes{};
    uint8_t size = 0;
    uint32_t session = 0;
};

// Main-loop API. A nonzero generation identifies one secure, subscribed session.
class Transport {
   public:
    virtual ~Transport() = default;
    [[nodiscard]] virtual uint32_t session() const = 0;
    virtual bool receive(Packet& packet) = 0;
    virtual bool send(uint32_t session, std::span<const uint8_t> bytes) = 0;
};

}  // namespace platform::rpc
