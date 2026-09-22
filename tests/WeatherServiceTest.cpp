#include <cassert>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>

#include "platform/weather/services/WeatherService.h"

using namespace platform;

namespace {

class Peer final : public rpc::Transport {
   public:
    uint32_t generation = 1;
    uint32_t now = 0;
    unsigned requests = 0;
    bool respond = true;
    uint8_t error = 0;
    std::vector<uint8_t> report{1, 113, 0, 251, 255, 12, 0, 8, 'S', 'h', 'a', 'n', 'g', 'h', 'a', 'i'};

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
        if (auto message = _channel.receive(bytes, now); message && message->kind == rpc::Kind::Request) {
            assert(message->method == weather::weatherGetMethod && message->payload.empty());
            ++requests;
            if (respond) {
                assert(_channel.enqueue({.kind = error ? rpc::Kind::Error : rpc::Kind::Response,
                                         .id = message->id,
                                         .method = message->method,
                                         .payload = error ? std::vector<uint8_t>{error} : report},
                                        now));
            }
        }
        pump();
        return true;
    }

    void connect(uint32_t session) {
        generation = session;
        _inbound.clear();
        _channel.reset();
        assert(_channel.enqueue({.id = 60000, .method = rpc::helloMethod, .payload = {2}}, now));
        pump();
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

   private:
    std::deque<rpc::Packet> _inbound;
    rpc::MessageChannel _channel;
};

struct Disk {
    std::array<std::optional<std::string>, 2> slots;
    unsigned writes = 0;
    bool writable = true;
    bool partial = false;
};

class Files final : public weather::WeatherFiles {
   public:
    explicit Files(std::shared_ptr<Disk> disk) : _disk(std::move(disk)) {
    }

    std::optional<std::string> read(unsigned slot) override {
        return _disk->slots[slot];
    }

    bool write(unsigned slot, std::string_view bytes) override {
        ++_disk->writes;
        if (!_disk->writable) return false;
        _disk->slots[slot] = std::string(bytes.substr(0, _disk->partial ? bytes.size() / 2 : bytes.size()));
        return !_disk->partial;
    }

   private:
    std::shared_ptr<Disk> _disk;
};

std::unique_ptr<weather::WeatherStorage> storage(const std::shared_ptr<Disk>& disk) {
    return std::make_unique<weather::FileWeatherStorage>(std::make_unique<Files>(disk));
}

struct Fixture {
    Peer peer;
    tasking::TaskDispatchService tasks;
    rpc::RPCService rpc{peer, tasks};
    std::shared_ptr<Disk> disk = std::make_shared<Disk>();
    std::optional<uint32_t> today = 20260922;
    weather::WeatherService service{rpc, [this] { return today; }, storage(disk)};
    uint32_t now = 0;

    explicit Fixture(uint32_t initial = 0) : now(initial) {
        assert(tasks.start());
        tasks.update(now);
        assert(rpc.start() && service.start() && service.start());
        assert(!service.report());
        peer.now = now;
        peer.connect(1);
    }

    void tick(uint32_t delta = 10) {
        now += delta;
        peer.now = now;
        peer.pump();
        tasks.update(now);
        service.update(now);
    }

    void settle() {
        for (int i = 0; i < 20; ++i) tick();
    }
};

void lifecycleAndCadence() {
    Fixture f;
    f.settle();
    assert(f.peer.requests == 1 && f.service.report());
    assert(f.service.reportRevision() == 1);
    assert(f.service.report()->city == "Shanghai" && f.service.report()->minTempC == -5);
    f.tick(3599000);
    assert(f.peer.requests == 1);
    f.tick(1000);
    f.settle();
    assert(f.peer.requests == 2 && f.service.reportRevision() == 1);
    f.peer.error = 7;
    f.tick(3600000);
    f.settle();
    assert(f.service.syncState() == weather::WeatherService::SyncState::Failed);
    assert(f.service.report()->weatherCode == 113);
    auto count = f.peer.requests;
    f.tick(29000);
    assert(f.peer.requests == count);
    f.peer.error = 0;
    f.peer.report[1] = 119;
    f.tick(1000);
    f.settle();
    assert(f.service.report()->weatherCode == 119 && f.peer.requests == count + 1);
    assert(f.service.reportRevision() == 2);
    f.peer.generation = 0;
    f.tick();
    assert(f.service.syncState() == weather::WeatherService::SyncState::Waiting && f.service.report());
    f.peer.connect(2);
    f.settle();
    assert(f.peer.requests == count + 2);
    f.service.stop();
    f.service.stop();
    f.tick(3600000);
    assert(f.peer.requests == count + 2 && !f.service.isRunning());
    assert(f.service.start());
    f.settle();
    assert(f.peer.requests == count + 3);
}

void failuresAndRollover() {
    Fixture f(0xfffff000U);
    f.peer.respond = false;
    f.settle();
    assert(!f.service.report());
    f.tick(120000);
    assert(f.service.syncState() == weather::WeatherService::SyncState::Failed);
    f.peer.respond = true;
    f.tick(30000);
    f.settle();
    assert(f.service.report() && f.peer.requests == 2);
    f.peer.report[0] = 99;
    f.tick(3600000);
    f.settle();
    assert(f.service.syncState() == weather::WeatherService::SyncState::Failed && f.service.report());
    f.peer.report[0] = 1;
    f.peer.respond = false;
    f.tick(30000);
    f.settle();
    f.service.stop();
    f.tick(120000);
    assert(!f.service.isRunning());
}

void restoreAndDates() {
    Fixture f;
    f.settle();
    assert(f.disk->writes == 1);
    assert(f.service.persistenceState() == weather::WeatherService::PersistenceState::Saved);
    const auto value = f.service.report();
    f.service.stop();
    f.peer.respond = false;
    weather::WeatherService restored(f.rpc, [&] { return f.today; }, storage(f.disk));
    assert(restored.start() && restored.report() == value && restored.reportRevision() == 1);
    restored.update(f.now);
    assert(restored.syncState() == weather::WeatherService::SyncState::Pending);
    assert(restored.report() == value);
    f.today = 20260923;
    restored.update(f.now + 10);
    assert(!restored.report() && restored.reportRevision() == 2);
    restored.stop();
    weather::WeatherService nextDay(f.rpc, [&] { return f.today; }, storage(f.disk));
    assert(nextDay.start() && !nextDay.report());
    nextDay.stop();
    f.today.reset();
    weather::WeatherService unknownDate(f.rpc, [&] { return f.today; }, storage(f.disk));
    assert(unknownDate.start() && !unknownDate.report());
    unknownDate.update(f.now + 20);
    assert(unknownDate.syncState() == weather::WeatherService::SyncState::Waiting);
    f.today = 20260922;
    unknownDate.update(f.now + 30);
    assert(unknownDate.report() == value);
}

void interruptedStorageAndRetry() {
    Fixture f;
    f.disk->writable = false;
    f.settle();
    assert(f.service.report() && f.service.persistenceState() == weather::WeatherService::PersistenceState::Failed);
    f.disk->writable = true;
    f.tick(30000);
    assert(f.service.persistenceState() == weather::WeatherService::PersistenceState::Saved);
    const auto writes = f.disk->writes;
    f.tick(3600000);
    f.settle();
    assert(f.disk->writes == writes);
    auto diskStorage = storage(f.disk);
    const auto saved = diskStorage->load();
    assert(saved && saved->date == 20260922);
    auto changed = *saved;
    changed.report.weatherCode = 119;
    f.disk->partial = true;
    assert(!diskStorage->save(changed));
    assert(storage(f.disk)->load() == saved);
    f.disk->partial = false;
    assert(diskStorage->save(changed));
    assert(storage(f.disk)->load() == changed);
    // Corrupt the most recent slot; startup falls back to the older validated slot.
    (*f.disk->slots[1])[16] ^= 1;
    assert(storage(f.disk)->load() == saved);
    changed.date = 20260230;
    assert(!diskStorage->save(changed));
}

void midnightDuringRequest() {
    Fixture f;
    f.peer.respond = false;
    f.settle();
    assert(f.peer.requests == 1 && !f.service.report());
    f.today = 20260923;
    f.peer.respond = true;
    f.tick();
    f.settle();
    assert(f.service.report() && f.peer.requests == 2);
    assert(storage(f.disk)->load()->date == 20260923);
    f.peer.generation = 0;
    f.today = 20260924;
    f.tick();
    assert(!f.service.report());
}

void validation() {
    Peer peer;
    assert(weather::decodeWeatherReport(peer.report));
    for (size_t length = 0; length < peer.report.size(); ++length) {
        assert(!weather::decodeWeatherReport(std::span(peer.report).first(length)));
    }
    auto invalid = peer.report;
    invalid[1] = 0;
    assert(!weather::decodeWeatherReport(invalid));
    invalid = peer.report;
    invalid[3] = 20;
    invalid[4] = 0;
    assert(!weather::decodeWeatherReport(invalid));
    invalid = peer.report;
    invalid[8] = 0xc0;
    assert(!weather::decodeWeatherReport(invalid));
}

}  // namespace

int main(int argc, char** argv) {
    restoreAndDates();
    interruptedStorageAndRetry();
    midnightDuringRequest();
    validation();
    lifecycleAndCadence();
    failuresAndRollover();
    if (argc == 2) {
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
        const auto report = weather::decodeWeatherReport(bytes);
        assert(report && report->city == "上海" && report->weatherCode == 113);
        assert(report->minTempC == -5 && report->maxTempC == 12);
    }
    std::cout << "Weather wire, cadence, persistent cache, date rollover, recovery and lifecycle checks passed\n";
}
