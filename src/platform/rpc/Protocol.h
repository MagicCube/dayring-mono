#pragma once
#include <algorithm>
#include <optional>

#include "Transport.h"

namespace platform::rpc {

enum class Kind : uint8_t { Request = 0, Response = 1, Error = 2 };
enum class Error : uint8_t {
    None = 0,
    UnknownMethod = 1,
    InvalidPayload = 2,
    Busy = 3,
    Timeout = 4,
    Disconnected = 5,
    Cancelled = 6
};
inline constexpr uint16_t helloMethod = 0;
inline constexpr uint16_t pingMethod = 1;
inline constexpr uint16_t clockGetMethod = 2;
inline constexpr uint16_t timeZoneGetMethod = 3;
inline constexpr uint16_t clockChangedMethod = 4;
inline constexpr uint16_t clockStatusMethod = 5;

struct Message {
    Kind kind = Kind::Request;
    uint16_t id = 0;
    uint16_t method = 0;
    std::array<uint8_t, 12> payload{};
    uint8_t size = 0;

    [[nodiscard]] std::span<const uint8_t> data() const {
        return {payload.data(), size};
    }
};

inline Packet encode(const Message& message) {
    Packet packet;
    if (message.size > 12 || message.id == 0) return packet;
    packet.size = 8 + message.size;
    packet.bytes[0] = 0xD1;
    packet.bytes[1] = static_cast<uint8_t>(message.kind);
    packet.bytes[2] = message.id;
    packet.bytes[3] = message.id >> 8;
    packet.bytes[4] = message.method;
    packet.bytes[5] = message.method >> 8;
    packet.bytes[6] = message.size;
    packet.bytes[7] = 0;
    std::copy_n(message.payload.begin(), message.size, packet.bytes.begin() + 8);
    return packet;
}

inline std::optional<Message> decode(std::span<const uint8_t> bytes) {
    if (bytes.size() < 8 || bytes[0] != 0xD1 || bytes[1] > 2 || bytes[6] > 12 || bytes[7] != 0 ||
        bytes.size() != 8U + bytes[6])
        return std::nullopt;
    Message message{.kind = static_cast<Kind>(bytes[1]),
                    .id = static_cast<uint16_t>(bytes[2] | bytes[3] << 8),
                    .method = static_cast<uint16_t>(bytes[4] | bytes[5] << 8),
                    .size = bytes[6]};
    if (message.id == 0 || (message.kind == Kind::Error && (message.size != 1 || bytes[8] == 0))) return std::nullopt;
    std::copy(bytes.begin() + 8, bytes.end(), message.payload.begin());
    return message;
}

struct Reply {
    Error error = Error::None;
    std::array<uint8_t, 12> payload{};
    uint8_t size = 0;
};

}  // namespace platform::rpc
