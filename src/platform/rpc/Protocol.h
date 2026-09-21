#pragma once
#include <algorithm>
#include <optional>
#include <vector>

#include "Transport.h"

namespace platform::rpc {

inline constexpr size_t maxPayloadSize = 10 * 1024;
inline constexpr uint32_t messageTimeout = 120000;
enum class Kind : uint8_t { Request = 0, Response = 1, Error = 2 };
enum class Error : uint8_t {
    None = 0,
    UnknownMethod = 1,
    InvalidPayload = 2,
    Busy = 3,
    Timeout = 4,
    Disconnected = 5,
    Cancelled = 6,
    Internal = 7
};
inline constexpr uint16_t helloMethod = 0;
inline constexpr uint16_t pingMethod = 1;
inline constexpr uint16_t clockGetMethod = 2;
inline constexpr uint16_t timeZoneGetMethod = 3;
inline constexpr uint16_t clockChangedMethod = 4;
inline constexpr uint16_t clockStatusMethod = 5;
inline constexpr uint16_t pairingResetMethod = 6;
inline constexpr uint16_t deviceRebootMethod = 7;

struct Message {
    Kind kind = Kind::Request;
    uint16_t id = 0;
    uint16_t method = 0;
    std::vector<uint8_t> payload;

    [[nodiscard]] std::span<const uint8_t> data() const {
        return payload;
    }
};

// Version 1 remains the single-packet path for payloads up to 12 bytes.
inline Packet encode(const Message& message) {
    Packet packet;
    if (message.payload.size() > 12 || message.id == 0) return packet;
    packet.size = 8 + message.payload.size();
    packet.bytes[0] = 0xD1;
    packet.bytes[1] = static_cast<uint8_t>(message.kind);
    packet.bytes[2] = message.id;
    packet.bytes[3] = message.id >> 8;
    packet.bytes[4] = message.method;
    packet.bytes[5] = message.method >> 8;
    packet.bytes[6] = message.payload.size();
    std::copy(message.payload.begin(), message.payload.end(), packet.bytes.begin() + 8);
    return packet;
}

inline std::optional<Message> decode(std::span<const uint8_t> bytes) {
    if (bytes.size() < 8 || bytes[0] != 0xD1 || bytes[1] > 2 || bytes[6] > 12 || bytes[7] != 0 ||
        bytes.size() != 8U + bytes[6])
        return std::nullopt;
    Message message{.kind = static_cast<Kind>(bytes[1]),
                    .id = static_cast<uint16_t>(bytes[2] | bytes[3] << 8),
                    .method = static_cast<uint16_t>(bytes[4] | bytes[5] << 8),
                    .payload = {bytes.begin() + 8, bytes.end()}};
    if (message.id == 0 || (message.kind == Kind::Error && (bytes[6] != 1 || bytes[8] == 0))) return std::nullopt;
    return message;
}

struct Reply {
    Error error = Error::None;
    std::vector<uint8_t> payload;
};

}  // namespace platform::rpc
