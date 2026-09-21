#include <cassert>
#include <deque>
#include <iostream>

#include "platform/hal/RtcClock.h"
#include "platform/time/ClockSample.h"
#include "platform/time/services/TimeService.h"

using namespace platform;

namespace {

uint32_t nowMs = 0;
int writes = 0;
bool writeOK = true;
Rtc::DateTime clockValue{};

}  // namespace

unsigned long millis() {
    return nowMs;
}

namespace platform::hal {

Rtc::DateTime clockTime() {
    return clockValue;
}

bool setClockTime(const Rtc::DateTime& value) {
    ++writes;
    if (!writeOK) return false;
    clockValue = value;
    return true;
}

}  // namespace platform::hal

class ClockTransport final : public rpc::Transport {
   public:
    uint32_t generation = 1;
    int requests = 0;
    std::string zone = "Asia/Shanghai";
    int32_t offset = 28800;
    uint32_t handshakeSession = 0;
    bool respond = true, malformed = false;
    std::deque<rpc::Packet> input;

    uint32_t session() const override {
        return generation;
    }

    bool receive(rpc::Packet& packet) override {
        if (generation && handshakeSession != generation) {
            handshakeSession = generation;
            packet = rpc::encode({.id = 500, .method = rpc::helloMethod});
            packet.session = generation;
            return true;
        }
        if (input.empty()) return false;
        packet = input.front();
        input.pop_front();
        return true;
    }

    bool send(uint32_t session, std::span<const uint8_t> bytes) override {
        auto request = rpc::decode(bytes);
        assert(request);
        if (request->kind != rpc::Kind::Request) return true;
        if (request->method == rpc::timeZoneGetMethod) {
            auto chunk = std::string_view(zone).substr(request->payload[0], 12);
            rpc::Message reply{.kind = rpc::Kind::Response,
                               .id = request->id,
                               .method = request->method,
                               .size = static_cast<uint8_t>(chunk.size())};
            std::copy(chunk.begin(), chunk.end(), reply.payload.begin());
            auto packet = rpc::encode(reply);
            packet.session = session;
            input.push_back(packet);
            return true;
        }
        assert(request->method == rpc::clockGetMethod);
        ++requests;
        if (respond) {
            // 2024-02-29 23:59:59 UTC + 8 hours = March 1, Friday.
            uint64_t epoch = 1709251199;
            rpc::Message reply{.kind = rpc::Kind::Response,
                               .id = request->id,
                               .method = request->method,
                               .size = static_cast<uint8_t>(malformed ? 0 : 12)};
            for (int i = 0; i < 8; ++i) reply.payload[i] = epoch >> (8 * i);
            const uint32_t rawOffset = static_cast<uint32_t>(offset);
            for (int i = 0; i < 4; ++i) reply.payload[8 + i] = rawOffset >> (8 * i);
            auto packet = rpc::encode(reply);
            packet.session = session;
            input.push_back(packet);
        }
        return true;
    }
};

int main() {
    ClockTransport transport;
    tasking::TaskDispatchService tasks;
    rpc::RPCService rpc(transport, tasks);
    time::TimeService time(&rpc);
    assert(tasks.start() && rpc.start() && time.start());
    auto tick = [&](uint32_t now) {
        nowMs = now;
        tasks.update(now);
        time.update(now);
    };
    tick(0);
    assert(transport.requests == 1 && writes == 0);
    tick(10);
    assert(writes == 1 && time.syncState() == time::TimeService::SyncState::Synchronized);
    assert(time.hasTimeZone() && time.timeZone() == "Asia/Shanghai" && time.utcOffsetSeconds() == 28800);
    assert(clockValue.year == 2024 && clockValue.month == 3 && clockValue.day == 1 && clockValue.hour == 7 &&
           clockValue.weekday == 5);
    constexpr uint32_t interval = 8U * 60 * 60 * 1000;
    tick(interval + 9);
    assert(transport.requests == 1);
    tick(interval + 10);
    assert(transport.requests == 2);
    tick(interval + 20);
    assert(writes == 2);
    transport.generation = 0;
    tick(interval + 30);
    transport.generation = 2;
    tick(interval + 40);
    assert(transport.requests == 3);
    tick(interval + 50);
    assert(writes == 3);
    transport.generation = 3;
    transport.malformed = true;
    tick(interval + 60);
    tick(interval + 70);
    assert(writes == 3 && time.syncState() == time::TimeService::SyncState::Failed);
    tick(interval + 30059);
    assert(transport.requests == 4);
    transport.malformed = false;
    writeOK = false;
    tick(interval + 30060);
    tick(interval + 30070);
    assert(time.syncState() == time::TimeService::SyncState::Failed);
    writeOK = true;
    transport.respond = false;
    transport.generation = 4;
    tick(interval + 30080);
    tick(interval + 35080);
    assert(time.syncState() == time::TimeService::SyncState::Failed);
    const auto before = transport.requests;
    time.stop();
    tick(interval * 2);
    assert(transport.requests == before);
    transport.respond = true;
    assert(time.start());
    tick(interval * 2 + 10);
    tick(interval * 2 + 20);
    assert(time.syncState() == time::TimeService::SyncState::Synchronized);
    const auto previousWrites = writes;
    transport.zone = "America/New_York";
    transport.offset = -18000;
    auto changed = rpc::encode({.id = 100, .method = rpc::clockChangedMethod});
    changed.session = transport.generation;
    transport.input.push_back(changed);
    tick(interval * 2 + 30);
    tick(interval * 2 + 40);
    assert(writes == previousWrites + 1 && time.timeZone() == "America/New_York" && time.utcOffsetSeconds() == -18000);
    assert(clockValue.month == 2 && clockValue.day == 29 && clockValue.hour == 18);
    // A zone change during an in-flight sync invalidates that sample.
    transport.generation++;
    tick(interval * 2 + 50);
    changed.session = transport.generation;
    transport.input.push_front(changed);
    const auto beforeChange = writes;
    tick(interval * 2 + 60);
    assert(writes == beforeChange);
    tick(interval * 2 + 70);
    assert(writes == beforeChange + 1);
    time.stop();
    rpc.stop();
    tasks.stop();
    assert(!time::decodeClockSample(std::array<uint8_t, 12>{}));
    std::cout
        << "Immediate/reconnect/eight-hour RTC sync, malformed response, write failure, timeout and stop passed\n";
}
