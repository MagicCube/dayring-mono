#pragma once

#include "Protocol.h"

namespace platform::rpc {

// D2, kind (bit 7 = acknowledgment), id, method, total length, byte offset; all u16 LE.
struct Fragment {
    Kind kind;
    uint16_t id, method, total, offset;
    bool acknowledgment = false;
    std::span<const uint8_t> payload;
};

inline std::optional<Fragment> decodeFragment(std::span<const uint8_t> bytes) {
    if (bytes.size() < 10 || bytes.size() > maxPacketSize || bytes[0] != 0xD2 || (bytes[1] & 0x7F) > 1)
        return std::nullopt;
    auto word = [&](size_t at) { return static_cast<uint16_t>(bytes[at] | bytes[at + 1] << 8); };
    Fragment f{static_cast<Kind>(bytes[1] & 0x7F),
               word(2),
               word(4),
               word(6),
               word(8),
               (bytes[1] & 0x80) != 0,
               bytes.subspan(10)};
    if (!f.id || f.total <= 12 || f.total > maxPayloadSize || f.offset > f.total) return std::nullopt;
    if (f.acknowledgment ? (!f.payload.empty() || !f.offset)
                         : (f.payload.empty() || f.offset + f.payload.size() > f.total))
        return std::nullopt;
    return f;
}

inline Packet encodeFragment(const Fragment& f) {
    Packet p;
    if (f.payload.size() > maxPacketSize - 10) return p;
    p.size = 10 + f.payload.size();
    p.bytes[0] = 0xD2;
    p.bytes[1] = static_cast<uint8_t>(f.kind) | (f.acknowledgment ? 0x80 : 0);
    const std::array<uint16_t, 4> words{f.id, f.method, f.total, f.offset};
    for (size_t i = 0; i < words.size(); ++i) {
        p.bytes[2 + i * 2] = words[i];
        p.bytes[3 + i * 2] = words[i] >> 8;
    }
    std::copy(f.payload.begin(), f.payload.end(), p.bytes.begin() + 10);
    return p;
}

}  // namespace platform::rpc
