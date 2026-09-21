#include <cassert>
#include <deque>
#include <iostream>
#include <limits>

#include "platform/ble/RPCMailbox.h"
#include "platform/rpc/services/RPCService.h"

using namespace platform::rpc;
using platform::tasking::TaskDispatchService;

class FakeTransport final : public Transport {
   public:
    uint32_t generation = 1;
    bool writable = true;
    std::deque<Packet> inbound, outbound;

    uint32_t session() const override {
        return generation;
    }

    bool receive(Packet& p) override {
        if (inbound.empty()) return false;
        p = inbound.front();
        inbound.pop_front();
        return true;
    }

    bool send(uint32_t session, std::span<const uint8_t> bytes) override {
        if (!writable || !generation || session != generation) return false;
        Packet p;
        p.session = session;
        p.size = bytes.size();
        std::copy(bytes.begin(), bytes.end(), p.bytes.begin());
        outbound.push_back(p);
        return true;
    }

    void inject(Message m, uint32_t session = 1) {
        auto p = encode(m);
        p.session = session;
        inbound.push_back(p);
    }
};

int main() {
    auto valid = encode({.id = 0x1234, .method = 1});
    assert(valid.size == 8 && valid.bytes[0] == 0xD1 && valid.bytes[2] == 0x34 && valid.bytes[3] == 0x12);
    assert(decode({valid.bytes.data(), valid.size}));
    valid.bytes[7] = 1;
    assert(!decode({valid.bytes.data(), valid.size}));
    FakeTransport transport;
    TaskDispatchService tasks;
    RPCService rpc(transport, tasks);
    assert(!rpc.start());
    assert(tasks.start() && rpc.start());
    transport.inject({.id = 500, .method = helloMethod});
    tasks.update(0);
    assert(rpc.session() == 1);
    Error result = Error::None;
    int callbacks = 0;
    auto id = rpc.request(
        1, {},
        [&](Error e, auto) {
            result = e;
            ++callbacks;
        },
        0, 100);
    assert(id != 0);
    transport.inject({.kind = Kind::Response, .id = id, .method = 2});
    tasks.update(10);
    assert(callbacks == 0);
    transport.inject({.kind = Kind::Response, .id = id, .method = 1});
    tasks.update(20);
    assert(callbacks == 1 && result == Error::None);
    transport.inject({.kind = Kind::Response, .id = id, .method = 1});
    tasks.update(30);
    assert(callbacks == 1);
    transport.inject({.id = 99, .method = 1, .payload = {7, 8}, .size = 2});
    tasks.update(40);
    auto reply = decode({transport.outbound.back().bytes.data(), transport.outbound.back().size});
    assert(reply && reply->kind == Kind::Response && reply->payload[1] == 8);
    transport.inject({.id = 100, .method = 999});
    tasks.update(50);
    reply = decode({transport.outbound.back().bytes.data(), transport.outbound.back().size});
    assert(reply->kind == Kind::Error && reply->payload[0] == 1);
    id = rpc.request(1, {}, [&](Error e, auto) { result = e; }, 50, 50);
    transport.inject({.kind = Kind::Response, .id = id, .method = 1});
    tasks.update(100);
    assert(result == Error::Timeout);
    id = rpc.request(1, {}, [&](Error e, auto) { result = e; }, 100);
    transport.generation = 0;
    tasks.update(110);
    assert(result == Error::Disconnected);
    transport.generation = 2;
    transport.inject({.id = 500, .method = helloMethod}, 2);
    tasks.update(120);
    callbacks = 0;
    const auto newID = rpc.request(1, {}, [&](Error, auto) { ++callbacks; }, 120);
    transport.inject({.kind = Kind::Response, .id = newID, .method = 1}, 1);
    tasks.update(130);
    assert(callbacks == 0);
    assert(rpc.cancel(newID) && callbacks == 1 && !rpc.cancel(newID));
    transport.writable = false;
    assert(rpc.request(1, {}, [](auto, auto) {}, 130) == 0);
    transport.writable = true;
    for (int i = 0; i < 8; ++i) assert(rpc.request(1, {}, [](auto, auto) {}, 130));
    assert(!rpc.request(1, {}, [](auto, auto) {}, 130));
    rpc.stop();
    assert(!rpc.isRunning());
    tasks.update(140);
    assert(tasks.size() == 0);
    assert(rpc.start());
    transport.inject({.id = 500, .method = helloMethod}, 2);
    tasks.update(150);
    const auto wrapStart = std::numeric_limits<uint32_t>::max() - 20;
    tasks.update(wrapStart);
    assert(rpc.request(1, {}, [&](Error e, auto) { result = e; }, wrapStart, 40));
    tasks.update(19);
    assert(result == Error::Timeout);
    rpc.stop();
    tasks.stop();
    platform::ble::RPCMailbox mailbox;
    mailbox.reset(true);
    auto generation = mailbox.session();
    for (int i = 0; i < 8; ++i) assert(mailbox.send(generation, std::array<uint8_t, 1>{1}));
    assert(!mailbox.send(generation, std::array<uint8_t, 1>{1}));
    mailbox.reset(false);
    mailbox.reset(true);
    Packet p;
    assert(!mailbox.transmit(p) && !mailbox.send(generation, std::array<uint8_t, 1>{1}));
    std::cout << "RPC framing, dispatch, timeout, cancellation, session isolation and bounded queues passed\n";
}
