#include "MessageChannel.h"

namespace platform::rpc {

bool MessageChannel::enqueue(Message message, uint32_t now, uint32_t timeout) {
    if (_failed || message.payload.size() > maxPayloadSize) return false;
    if (message.payload.size() <= 12) {
        if (_small.size() >= (_replyReserved ? 7U : 8U)) return false;
        _small.push_back({encode(message), now});
    } else {
        if (_outgoing.size() >= (_replyReserved ? 7U : 8U) ||
            _queuedBytes + message.payload.size() > (_replyReserved ? 1U : 2U) * maxPayloadSize)
            return false;
        _queuedBytes += message.payload.size();
        _outgoing.push_back({std::move(message), now, timeout});
    }
    return true;
}

void MessageChannel::_acknowledge(const Fragment& f) {
    if (_outgoing.empty()) return;
    auto& tx = _outgoing.front();
    if (f.kind != tx.message.kind || f.id != tx.message.id || f.method != tx.message.method ||
        f.total != tx.message.payload.size() || !tx.awaiting || f.offset != tx.awaiting)
        return;
    tx.offset = tx.awaiting;
    tx.awaiting = 0;
    if (tx.offset == tx.message.payload.size()) {
        _queuedBytes -= tx.message.payload.size();
        _outgoing.pop_front();
    }
}

std::optional<Message> MessageChannel::receive(std::span<const uint8_t> bytes, uint32_t now) {
    if (_failed || bytes.empty()) return std::nullopt;
    if (bytes[0] == 0xD1) return decode(bytes);
    auto f = decodeFragment(bytes);
    if (!f) {
        _failed = true;
        return std::nullopt;
    }
    if (f->acknowledgment) {
        _acknowledge(*f);
        return std::nullopt;
    }
    if (_ack) {
        _failed = true;
        return std::nullopt;
    }
    if (f->offset == 0) {
        // A new message replaces an incomplete transfer cancelled by its sender.
        _incoming = Message{.kind = f->kind, .id = f->id, .method = f->method};
        _incoming->payload.reserve(f->total);
        _total = f->total;
    }
    if (!_incoming || _incoming->kind != f->kind || _incoming->id != f->id || _incoming->method != f->method ||
        _total != f->total || _incoming->payload.size() != f->offset) {
        _failed = true;
        return std::nullopt;
    }
    _incoming->payload.insert(_incoming->payload.end(), f->payload.begin(), f->payload.end());
    _receivedAt = now;
    f->offset = _incoming->payload.size();
    f->acknowledgment = true;
    f->payload = {};
    _ack = Small{encodeFragment(*f), now};
    if (_incoming->payload.size() != _total) return std::nullopt;
    auto message = std::move(_incoming);
    _incoming.reset();
    return message;
}

void MessageChannel::pump(uint32_t now, size_t packetSize, const Send& send) {
    if (_failed) return;
    if (_incoming && now - _receivedAt >= 5000U) _incoming.reset();
    if (!_outgoing.empty() && (now - _outgoing.front().started >= _outgoing.front().timeout ||
                               (_outgoing.front().awaiting && now - _outgoing.front().sentAt >= 5000U))) {
        _failed = true;
        return;
    }
    auto transmit = [&](const Small& item) {
        if (now - item.started >= 5000U) {
            _failed = true;
            return false;
        }
        return send({item.packet.bytes.data(), item.packet.size});
    };
    if (_ack) {
        if (!transmit(*_ack)) return;
        _ack.reset();
    }
    for (int budget = 0; budget < 4 && !_small.empty(); ++budget) {
        if (!transmit(_small.front())) return;
        _small.pop_front();
    }
    if (!_small.empty() || _outgoing.empty()) return;
    auto& tx = _outgoing.front();
    if (tx.awaiting) return;
    const size_t capacity = std::clamp(packetSize, size_t{20}, maxPacketSize) - 10;
    const auto count = std::min(capacity, tx.message.payload.size() - tx.offset);
    const auto p = encodeFragment({tx.message.kind, tx.message.id, tx.message.method,
                                   static_cast<uint16_t>(tx.message.payload.size()), tx.offset, false,
                                   tx.message.data().subspan(tx.offset, count)});
    if (send({p.bytes.data(), p.size})) {
        tx.awaiting = tx.offset + count;
        tx.sentAt = now;
    }
}

bool MessageChannel::reserveReply() {
    if (_replyReserved || _failed || _queuedBytes > maxPayloadSize || _outgoing.size() == 8 || _small.size() == 8)
        return false;
    _replyReserved = true;
    return true;
}

void MessageChannel::releaseReply() {
    _replyReserved = false;
}

void MessageChannel::cancel(uint16_t id) {
    std::erase_if(_outgoing, [&](const auto& tx) {
        if (tx.message.kind != Kind::Request || tx.message.id != id) return false;
        _queuedBytes -= tx.message.payload.size();
        return true;
    });
    std::erase_if(_small, [&](const auto& tx) {
        const auto m = decode({tx.packet.bytes.data(), tx.packet.size});
        return m && m->kind == Kind::Request && m->id == id;
    });
}

void MessageChannel::reset() {
    _outgoing.clear();
    _small.clear();
    _ack.reset();
    _incoming.reset();
    _queuedBytes = 0;
    _replyReserved = false;
    _failed = false;
}

}  // namespace platform::rpc
