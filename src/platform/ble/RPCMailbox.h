#pragma once
#include <mutex>

#include "../rpc/Transport.h"

namespace platform::ble {

// Two bounded queues bridge the NimBLE host and the owning application loop.
class RPCMailbox {
   public:
    uint32_t session() const {
        std::lock_guard lock(_mutex);
        return _session;
    }

    void reset(bool ready) {
        std::lock_guard lock(_mutex);
        _session = ready ? ++_generation : 0;
        if (ready && !_session) _session = ++_generation;
        _rx = {};
        _tx = {};
    }

    bool send(uint32_t session, std::span<const uint8_t> bytes) {
        std::lock_guard lock(_mutex);
        if (!session || session != _session) return false;
        return _tx.push(session, bytes);
    }

    bool deliver(std::span<const uint8_t> bytes) {
        std::lock_guard lock(_mutex);
        return _session && _rx.push(_session, bytes);
    }

    bool receive(rpc::Packet& packet) {
        std::lock_guard lock(_mutex);
        return _rx.pop(packet);
    }

    bool transmit(rpc::Packet& packet) {
        std::lock_guard lock(_mutex);
        return _tx.pop(packet);
    }

   private:
    struct Queue {
        std::array<rpc::Packet, 8> packets{};
        size_t head = 0, size = 0;

        bool push(uint32_t session, std::span<const uint8_t> bytes) {
            if (size == packets.size() || bytes.size() > 20 || bytes.empty()) return false;
            auto& packet = packets[(head + size++) % packets.size()];
            packet.session = session;
            packet.size = bytes.size();
            std::copy(bytes.begin(), bytes.end(), packet.bytes.begin());
            return true;
        }

        bool pop(rpc::Packet& packet) {
            if (!size) return false;
            packet = packets[head];
            head = (head + 1) % packets.size();
            --size;
            return true;
        }
    };

    mutable std::mutex _mutex;
    Queue _rx, _tx;
    uint32_t _session = 0, _generation = 0;
};

}  // namespace platform::ble
