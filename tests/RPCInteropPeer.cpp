#include <cassert>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "platform/rpc/services/RPCService.h"

using namespace platform::rpc;

namespace {

std::vector<uint8_t> sample() {
    std::vector<uint8_t> data(maxPayloadSize);
    for (size_t i = 0; i < data.size(); ++i) data[i] = i % 251;
    return data;
}

class PipeTransport final : public Transport {
   public:
    size_t limit = 20;
    std::deque<Packet> incoming;

    uint32_t session() const override {
        return 1;
    }

    size_t packetSize() const override {
        return limit;
    }

    bool receive(Packet& packet) override {
        if (incoming.empty()) return false;
        packet = incoming.front();
        incoming.pop_front();
        return true;
    }

    bool send(uint32_t generation, std::span<const uint8_t> bytes) override {
        assert(generation == 1 && bytes.size() <= limit);
        std::cout << "FRAME ";
        for (auto byte : bytes) std::cout << std::hex << std::setw(2) << std::setfill('0') << int(byte);
        std::cout << std::dec << '\n';
        return true;
    }

    void disconnect() override {
        assert(false && "unexpected disconnect");
    }
};

}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    PipeTransport transport;
    transport.limit = std::stoi(argv[1]);
    platform::tasking::TaskDispatchService tasks;
    RPCService peer(transport, tasks);
    assert(tasks.start() && peer.start());
    const auto data = sample();
    int handled = 0;
    bool completed = false;
    peer.registerHandler(42, [&](auto bytes) {
        assert(std::vector<uint8_t>(bytes.begin(), bytes.end()) == data);
        ++handled;
        return Reply{.payload = data};
    });
    uint32_t now = 0;
    for (std::string line; std::getline(std::cin, line);) {
        if (line == "start") {
            assert(peer.request(
                42, data,
                [&](Error error, auto bytes) {
                    assert(error == Error::None && std::vector<uint8_t>(bytes.begin(), bytes.end()) == data);
                    completed = true;
                    std::cout << "COMPLETE\n";
                },
                now));
        } else if (line == "finish") {
            assert(completed && handled == 1);
            std::cout << "PASS\nEND\n" << std::flush;
            return 0;
        } else {
            if (line != "tick") {
                Packet packet;
                packet.session = 1;
                assert(line.size() % 2 == 0 && line.size() / 2 <= packet.bytes.size());
                packet.size = line.size() / 2;
                for (size_t i = 0; i < packet.size; ++i)
                    packet.bytes[i] = std::stoi(line.substr(i * 2, 2), nullptr, 16);
                transport.incoming.push_back(packet);
            }
            now += 10;
            tasks.update(now);
        }
        std::cout << "END\n" << std::flush;
    }
    return 1;
}
