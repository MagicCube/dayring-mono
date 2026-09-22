#include <cassert>
#include <iostream>
#include <vector>

#include "platform/runtime/services/ServiceManager.h"

using platform::runtime::Service;
using platform::runtime::ServiceManager;
using namespace platform::tasking;

namespace platform::hal {

void testFrontlightBrightness(uint8_t) {
}

}  // namespace platform::hal

namespace {

class PendingTask final : public Task {
   public:
    ExecutionResult execute(const TaskContext&) override {
        return ExecutionResult::Wait;
    }
};

class ProbeService final : public Service {
   public:
    ProbeService(ServiceManager& manager, std::vector<int>& events, int id, bool& fail)
        : _manager(manager), _events(events), _id(id), _fail(fail) {
    }

    ~ProbeService() override {
        _events.push_back(100 + _id);
    }

    bool start() override {
        assert(_manager.tasks().isRunning());
        assert(_manager.frontlight().isRunning() && _manager.power().isRunning() && _manager.time().isRunning());
        assert(_manager.ble().isRunning());
        assert(_manager.calendar().isRunning());
        assert(_manager.weather().isRunning() && !_manager.weather().report());
        assert(_manager.ble().state() == platform::ble::BLEService::State::Unavailable);
        assert(!_manager.begin());
        _events.push_back(_id);
        if (_fail) return false;
        _handle = std::move(_manager.tasks().submit(std::make_unique<PendingTask>()).handle);
        assert(_handle);
        return true;
    }

    void update(std::uint32_t now) override {
        assert(now == 42);
        assert(!_manager.begin());
        _manager.update(now);
        _events.push_back(10 + _id);
    }

    void stop() override {
        assert(_manager.tasks().isRunning());
        assert(_manager.frontlight().isRunning() && _manager.power().isRunning() && _manager.time().isRunning());
        _events.push_back(-_id);
        _handle.cancel();
    }

   private:
    ServiceManager& _manager;
    std::vector<int>& _events;
    int _id;
    bool& _fail;
    TaskHandle _handle;
};

void orderedLifecycle() {
    std::vector<int> events;
    bool fail = false;
    {
        ServiceManager manager;
        assert(!manager.isRunning());
        assert(!manager.tasks().isRunning());
        assert(!manager.calendar().isRunning());
        assert(!manager.frontlight().isRunning() && !manager.power().isRunning() && !manager.time().isRunning());
        assert(!manager.add(nullptr));
        assert(manager.add(std::make_unique<ProbeService>(manager, events, 1, fail)));
        assert(manager.add(std::make_unique<ProbeService>(manager, events, 2, fail)));
        assert(manager.begin() && manager.begin());
        assert(manager.isRunning());
        assert((events == std::vector<int>{1, 2}));
        manager.update(42);
        assert((events == std::vector<int>{1, 2, 11, 12}));
        events = {1, 2};
        auto retained = manager.tasks().submit(std::make_unique<PendingTask>());
        manager.stop();
        manager.stop();
        manager.update(42);
        assert(!manager.isRunning() && !manager.tasks().isRunning());
        assert(!manager.frontlight().isRunning() && !manager.power().isRunning() && !manager.time().isRunning());
        assert(retained.handle.status() == TaskStatus::Cancelled);
        assert(manager.tasks().size() == 0);
        assert((events == std::vector<int>{1, 2, -2, -1}));
        assert(!manager.add(std::make_unique<ProbeService>(manager, events, 3, fail)));
        assert(manager.begin());
        events.clear();
    }
    assert((events == std::vector<int>{-2, -1, 102, 101}));
}

void failedStartup() {
    std::vector<int> events;
    bool success = false;
    bool failure = true;
    ServiceManager manager;
    assert(manager.add(std::make_unique<ProbeService>(manager, events, 1, success)));
    assert(manager.add(std::make_unique<ProbeService>(manager, events, 2, failure)));
    assert(manager.add(std::make_unique<ProbeService>(manager, events, 3, success)));
    assert(!manager.begin());
    assert((events == std::vector<int>{1, 2, -1}));
    assert(!manager.tasks().isRunning() && manager.tasks().size() == 0);
    assert(!manager.ble().isRunning());
    assert(manager.ble().state() == platform::ble::BLEService::State::Stopped);
    manager.stop();
    failure = false;
    events.clear();
    assert(manager.begin());
    assert((events == std::vector<int>{1, 2, 3}));
}

}  // namespace

int main() {
    orderedLifecycle();
    failedStartup();
    std::cout << "Service lifecycle tests passed\n";
}
