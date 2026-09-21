#pragma once

#include <cassert>
#include <deque>
#include <memory>
#include <string>

#include "platform/calendar/services/CalendarService.h"

namespace calendar_test {

using namespace platform;

inline calendar::ClockSample clockAt(std::string_view text) {
    auto value = calendar::parseDateTime(text);
    assert(value);
    return {value->localSeconds(), value->offsetSeconds, "Asia/Shanghai"};
}

inline std::string event(std::string id, std::string start, std::string end, bool allDay = false,
                         std::string title = "Meeting") {
    return "{\"instanceId\":\"" + id + "\",\"title\":\"" + title + "\",\"location\":\"Room\",\"start\":\"" + start +
           "\",\"end\":\"" + end + "\",\"isAllDay\":" + (allDay ? "true" : "false") + "}";
}

inline std::string snapshot(std::string events, std::string date = "2026-09-21", std::string end = "2026-09-23") {
    return "{\"schemaVersion\":1,\"generatedAt\":\"" + date +
           "T12:00:00+08:00\",\"timeZone\":\"Asia/Shanghai\",\"windowStart\":\"" + date +
           "T00:00:00+08:00\",\"windowEndExclusive\":\"" + end + "T00:00:00+08:00\",\"events\":[" + events + "]}";
}

inline std::string usualEvents() {
    return event("past", "2026-09-21T10:00:00+08:00", "2026-09-21T11:00:00+08:00") + "," +
           event("ongoing", "2026-09-21T11:30:00+08:00", "2026-09-21T13:00:00+08:00") + "," +
           event("future", "2026-09-21T14:00:00+08:00", "2026-09-21T15:00:00+08:00") + "," +
           event("all-day", "2026-09-21T00:00:00+08:00", "2026-09-22T00:00:00+08:00", true) + "," +
           event("tomorrow", "2026-09-22T18:00:00+08:00", "2026-09-22T19:00:00+08:00");
}

struct Disk {
    std::array<std::optional<std::string>, 2> slots;
    bool writable = true;
    bool interrupt = false;
    int writes = 0;
};

class Files final : public calendar::CalendarFiles {
   public:
    explicit Files(std::shared_ptr<Disk> disk) : _disk(std::move(disk)) {
    }

    std::optional<std::string> read(unsigned slot) override {
        return _disk->slots[slot];
    }

    bool write(unsigned slot, std::string_view bytes) override {
        ++_disk->writes;
        if (!_disk->writable) return false;
        _disk->slots[slot] = std::string(bytes.substr(0, _disk->interrupt ? bytes.size() / 2 : bytes.size()));
        return !_disk->interrupt;
    }

   private:
    std::shared_ptr<Disk> _disk;
};

inline std::unique_ptr<calendar::CalendarStorage> storage(std::shared_ptr<Disk> disk) {
    return std::make_unique<calendar::FileCalendarStorage>(std::make_unique<Files>(std::move(disk)));
}

class Peer final : public rpc::Transport {
   public:
    uint32_t generation = 1;
    uint32_t now = 0;
    std::string json = snapshot(usualEvents());
    int begins = 0, reads = 0;
    int busyCount = 0;
    bool respond = true;
    bool invalidManifest = false;
    bool shortPage = false;
    std::function<void()> onRead;
    std::deque<rpc::Message> replies;

    uint32_t session() const override {
        return generation;
    }

    size_t packetSize() const override {
        return 244;
    }

    bool receive(rpc::Packet& packet) override {
        if (_inbound.empty()) return false;
        packet = _inbound.front();
        _inbound.pop_front();
        return true;
    }

    bool send(uint32_t, std::span<const uint8_t> bytes) override {
        if (auto message = _channel.receive(bytes, now)) {
            if (message->kind == rpc::Kind::Request)
                _reply(*message);
            else
                replies.push_back(*message);
        }
        pump();
        return true;
    }

    void pump() {
        _channel.pump(now, 244, [this](auto bytes) {
            rpc::Packet packet;
            packet.session = generation;
            packet.size = bytes.size();
            std::copy(bytes.begin(), bytes.end(), packet.bytes.begin());
            _inbound.push_back(packet);
            return true;
        });
    }

    void connect(uint32_t session = 1) {
        generation = session;
        _inbound.clear();
        _channel.reset();
        assert(_channel.enqueue({.id = 60000, .method = rpc::helloMethod, .payload = {2}}, now));
        pump();
    }

    void status() {
        assert(_channel.enqueue({.id = _next++, .method = 11}, now));
        pump();
    }

    void changed(bool valid = true) {
        assert(_channel.enqueue(
            {.id = _next++, .method = 9, .payload = valid ? std::vector<uint8_t>{} : std::vector<uint8_t>{1}}, now));
        pump();
    }

   private:
    void _reply(const rpc::Message& request) {
        if (!respond) return;
        rpc::Message response{.kind = rpc::Kind::Response, .id = request.id, .method = request.method};
        std::string data;
        if (request.method == 8) {
            ++begins;
            if (busyCount > 0) {
                --busyCount;
                response.kind = rpc::Kind::Error;
                response.payload = {3};
                assert(_channel.enqueue(response, now));
                return;
            }
            _pinned = json;
            data = invalidManifest ? "{}"
                                   : "{\"snapshotId\":\"fixture-1\",\"byteLength\":" + std::to_string(_pinned.size()) +
                                         ",\"chunkBytes\":10240}";
        } else {
            assert(request.method == 10);
            ++reads;
            std::string query(request.payload.begin(), request.payload.end());
            const auto offset = static_cast<size_t>(std::stoul(query.substr(query.find("\"offset\":") + 9)));
            data = _pinned.substr(offset, rpc::maxPayloadSize);
            if (shortPage && !data.empty()) data.pop_back();
            if (onRead) {
                auto callback = std::move(onRead);
                onRead = {};
                callback();
            }
        }
        response.payload.assign(data.begin(), data.end());
        assert(_channel.enqueue(std::move(response), now));
    }

    uint16_t _next = 50000;
    rpc::MessageChannel _channel;
    std::deque<rpc::Packet> _inbound;
    std::string _pinned;
};

struct Fixture {
    Peer peer;
    tasking::TaskDispatchService tasks;
    rpc::RPCService rpc{peer, tasks};
    calendar::ClockSample clock = clockAt("2026-09-21T12:00:00+08:00");
    std::shared_ptr<Disk> disk = std::make_shared<Disk>();
    calendar::CalendarService service{rpc, [this] { return clock; }, storage(disk)};
    uint32_t now = 0;

    Fixture() {
        assert(tasks.start() && rpc.start() && service.start());
        peer.connect();
    }

    void tick(uint32_t delta = 10) {
        now += delta;
        peer.now = now;
        peer.pump();
        tasks.update(now);
        service.update(now);
    }

    void settle(unsigned ticks = 100) {
        for (unsigned i = 0; i < ticks; ++i) tick();
    }
};

}  // namespace calendar_test
