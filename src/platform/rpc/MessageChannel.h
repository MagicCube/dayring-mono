#pragma once

#include <deque>
#include <functional>

#include "Fragment.h"

namespace platform::rpc {

// One fragmented transfer in each direction. Short messages and ACKs bypass large messages.
class MessageChannel {
   public:
    using Send = std::function<bool(std::span<const uint8_t>)>;
    bool enqueue(Message message, uint32_t now, uint32_t timeout = messageTimeout);
    std::optional<Message> receive(std::span<const uint8_t> bytes, uint32_t now);
    void pump(uint32_t now, size_t packetSize, const Send& send);
    bool reserveReply();
    void releaseReply();
    void cancel(uint16_t id);
    void reset();

    [[nodiscard]] bool isFailed() const {
        return _failed;
    }

    [[nodiscard]] size_t queuedBytes() const {
        return _queuedBytes;
    }

   private:
    struct Outgoing {
        Message message;
        uint32_t started, timeout, sentAt = 0;
        uint16_t offset = 0, awaiting = 0;
    };

    struct Small {
        Packet packet;
        uint32_t started;
    };

    void _acknowledge(const Fragment& fragment);
    std::deque<Outgoing> _outgoing;
    std::deque<Small> _small;
    std::optional<Small> _ack;
    std::optional<Message> _incoming;
    uint16_t _total = 0;
    uint32_t _receivedAt = 0;
    size_t _queuedBytes = 0;
    bool _replyReserved = false;
    bool _failed = false;
};

}  // namespace platform::rpc
