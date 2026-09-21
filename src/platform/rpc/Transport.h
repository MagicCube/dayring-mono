#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace platform::rpc {

inline constexpr size_t maxPacketSize = 244;

struct Packet {
    std::array<uint8_t, maxPacketSize> bytes{};
    uint16_t size = 0;
    uint32_t session = 0;
};

// Main-loop API. A nonzero generation identifies one secure, subscribed session.
class Transport {
   public:
    virtual ~Transport() = default;
    [[nodiscard]] virtual uint32_t session() const = 0;

    virtual size_t packetSize() const {
        return 20;
    }

    virtual void disconnect() {
    }

    virtual bool receive(Packet& packet) = 0;
    virtual bool send(uint32_t session, std::span<const uint8_t> bytes) = 0;
};

}  // namespace platform::rpc
