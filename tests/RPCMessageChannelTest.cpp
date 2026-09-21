#include <cassert>
#include <deque>
#include <iostream>

#include "platform/rpc/MessageChannel.h"

using namespace platform::rpc;

namespace {

std::vector<uint8_t> payload(size_t size) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i) data[i] = i % 251;
    return data;
}

void exchange(size_t packetSize, size_t length) {
    MessageChannel a, b;
    const auto data = payload(length);
    assert(a.enqueue({.id = 1, .method = 42, .payload = data}, 0));
    assert(b.enqueue({.id = 1, .method = 42, .payload = data}, 0));
    std::deque<std::vector<uint8_t>> ab, ba;
    int requests = 0, responses = 0;
    auto sender = [&](auto& queue) {
        return [&](std::span<const uint8_t> bytes) {
            assert(bytes.size() <= packetSize);
            if (queue.size() == 1) return false;
            queue.emplace_back(bytes.begin(), bytes.end());
            return true;
        };
    };
    auto receive = [&](MessageChannel& peer, auto& queue, uint32_t now) {
        if (queue.empty()) return;
        auto bytes = std::move(queue.front());
        queue.pop_front();
        if (auto m = peer.receive(bytes, now)) {
            assert(m->payload == data);
            if (m->kind == Kind::Request) {
                ++requests;
                m->kind = Kind::Response;
                assert(peer.enqueue(std::move(*m), now));
            } else {
                ++responses;
            }
        }
    };
    for (uint32_t now = 0; now < messageTimeout && responses < 2; now += 10) {
        a.pump(now, packetSize, sender(ab));
        b.pump(now, packetSize, sender(ba));
        receive(a, ba, now);
        receive(b, ab, now);
        assert(!a.isFailed() && !b.isFailed());
    }
    assert(requests == 2 && responses == 2);
}

void boundsAndFlowControl() {
    MessageChannel channel;
    const auto data = payload(maxPayloadSize);
    assert(channel.enqueue({.id = 1, .payload = data}, 0));
    assert(channel.enqueue({.id = 2, .payload = data}, 0));
    assert(channel.queuedBytes() == 2 * maxPayloadSize);
    assert(!channel.reserveReply());
    assert(!channel.enqueue({.id = 3, .payload = payload(13)}, 0));
    assert(!channel.enqueue({.id = 4, .payload = payload(maxPayloadSize + 1)}, 0));
    std::vector<Packet> packets;
    const auto send = [&](auto bytes) {
        Packet packet;
        packet.size = bytes.size();
        std::copy(bytes.begin(), bytes.end(), packet.bytes.begin());
        packets.push_back(packet);
        return true;
    };
    channel.pump(0, 244, [](auto) { return false; });
    channel.pump(1, 244, send);
    channel.pump(2, 244, send);
    assert(packets.size() == 1);
    auto f = *decodeFragment({packets[0].bytes.data(), packets[0].size});
    f.acknowledgment = true;
    f.offset = f.payload.size();
    f.payload = {};
    f.kind = Kind::Response;
    auto ack = encodeFragment(f);
    channel.receive({ack.bytes.data(), ack.size}, 3);
    channel.pump(3, 244, send);
    assert(packets.size() == 1);  // Opposite-direction request IDs cannot acknowledge this transfer.
    f.kind = Kind::Request;
    f.offset--;
    ack = encodeFragment(f);
    channel.receive({ack.bytes.data(), ack.size}, 4);
    channel.pump(4, 244, send);
    assert(packets.size() == 1);
    f.offset++;
    ack = encodeFragment(f);
    channel.receive({ack.bytes.data(), ack.size}, 5);
    channel.pump(5, 244, send);
    assert(packets.size() == 2);
    assert(channel.enqueue({.id = 3, .payload = {42}}, 6));
    channel.pump(6, 244, send);
    assert(packets.back().bytes[0] == 0xD1);  // Small requests bypass a large transfer awaiting ACK.
    channel.cancel(1);
    assert(channel.queuedBytes() == maxPayloadSize);
    assert(channel.reserveReply());
    assert(!channel.enqueue({.id = 4, .payload = data}, 7));
    channel.releaseReply();
    assert(channel.enqueue({.kind = Kind::Response, .id = 4, .payload = data}, 7));
    channel.reset();
    assert(channel.queuedBytes() == 0 && !channel.isFailed());
}

void malformedAndExpiry() {
    MessageChannel channel;
    auto data = payload(13);
    auto first = encodeFragment({Kind::Request, 1, 42, 13, 0, false, std::span(data).first(10)});
    assert(first.bytes[0] == 0xD2 && first.bytes[6] == 13 && first.size == 20);
    assert(!channel.receive({first.bytes.data(), first.size}, 0));
    Packet ack;
    channel.pump(0, 20, [&](auto bytes) {
        ack.size = bytes.size();
        std::copy(bytes.begin(), bytes.end(), ack.bytes.begin());
        return true;
    });
    assert(ack.size == 10 && ack.bytes[1] == 0x80 && ack.bytes[8] == 10);
    auto wrong = encodeFragment({Kind::Request, 1, 42, 13, 11, false, std::span(data).last(2)});
    assert(!channel.receive({wrong.bytes.data(), wrong.size}, 1));
    assert(channel.isFailed());
    channel.reset();
    assert(!channel.receive({first.bytes.data(), first.size}, 0));
    channel.pump(0, 20, [](auto) { return true; });
    channel.pump(5000, 20, [](auto) { return true; });
    assert(!channel.isFailed());  // An abandoned partial request expires without invoking a handler.
    first.bytes[6] = 1;
    first.bytes[7] = 40;  // 10241, rejected before allocating.
    assert(!decodeFragment({first.bytes.data(), first.size}));
    channel.reset();
    assert(channel.enqueue({.id = 1, .payload = data}, 0));
    channel.pump(messageTimeout, 20, [](auto) { return false; });
    assert(channel.isFailed());
}

void cancellationAndStaleAcks() {
    MessageChannel sender, receiver;
    const auto data = payload(40);
    assert(sender.enqueue({.id = 1, .method = 42, .payload = data}, 0));
    Packet fragment, oldAck;
    auto capture = [](Packet& packet) {
        return [&packet](auto bytes) {
            packet.size = bytes.size();
            std::copy(bytes.begin(), bytes.end(), packet.bytes.begin());
            return true;
        };
    };
    sender.pump(0, 20, capture(fragment));
    assert(!receiver.receive({fragment.bytes.data(), fragment.size}, 0));
    receiver.pump(0, 20, capture(oldAck));
    sender.cancel(1);
    assert(sender.queuedBytes() == 0);
    assert(sender.enqueue({.id = 2, .method = 42, .payload = data}, 1));
    sender.pump(1, 20, capture(fragment));
    assert(!receiver.receive({fragment.bytes.data(), fragment.size}, 1));
    sender.receive({oldAck.bytes.data(), oldAck.size}, 1);
    int sent = 0;
    sender.pump(1, 20, [&](auto) {
        ++sent;
        return true;
    });
    assert(sent == 0 && !sender.isFailed() && !receiver.isFailed());
    // The current transfer completes after an earlier request was cancelled mid-message.
    for (uint32_t now = 2; now < 10; ++now) {
        Packet ack;
        receiver.pump(now, 20, capture(ack));
        sender.receive({ack.bytes.data(), ack.size}, now);
        sender.pump(now, 20, capture(fragment));
        if (auto message = receiver.receive({fragment.bytes.data(), fragment.size}, now)) {
            assert(message->id == 2 && message->payload == data);
            return;
        }
    }
    assert(false);
}

}  // namespace

int main() {
    for (const auto packetSize : {20, 64, 244})
        for (const auto size : {0, 12, 13, 234, 235, 10240}) exchange(packetSize, size);
    boundsAndFlowControl();
    malformedAndExpiry();
    cancellationAndStaleAcks();
    std::cout << "10 KiB simultaneous requests/replies, MTU boundaries, backpressure, bounds and malformed fragments "
                 "passed\n";
}
