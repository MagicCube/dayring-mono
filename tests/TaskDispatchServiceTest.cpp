#include <cassert>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

#include "platform/tasking/services/TaskDispatchService.h"

using namespace platform::tasking;

namespace {

class FunctionTask final : public Task {
   public:
    explicit FunctionTask(std::function<ExecutionResult(const TaskContext&)> fn) : _fn(std::move(fn)) {
    }

    ExecutionResult execute(const TaskContext& context) override {
        return _fn(context);
    }

   private:
    std::function<ExecutionResult(const TaskContext&)> _fn;
};

auto task(std::function<ExecutionResult(const TaskContext&)> fn) {
    return std::make_unique<FunctionTask>(std::move(fn));
}

auto complete() {
    return task([](const auto&) { return ExecutionResult::Complete; });
}

void queueAndLifetime() {
    TaskDispatchService manager(2);
    assert(manager.start());
    auto first = manager.submit(complete(), {.dedupeKey = "one"});
    assert(first.handle && first.error == SubmitError::None);
    assert(manager.submit(complete(), {.dedupeKey = "one"}).error == SubmitError::Duplicate);
    auto second = manager.submit(complete());
    assert(manager.submit(complete()).error == SubmitError::Full);
    assert(manager.cancel(first.handle.id()));
    assert(!manager.notify(first.handle.id()));
    assert(first.handle.status() == TaskStatus::Cancelled);
    {
        auto third = manager.submit(complete());
        assert(third.handle);
    }
    assert(manager.size() == 1);
    assert(manager.update(0, 10) == 1);
    assert(second.handle.status() == TaskStatus::Succeeded);
    assert(!manager.status(second.handle.id()));
    assert(manager.submit(nullptr).error == SubmitError::Invalid);
    TaskHandle survivor;
    {
        TaskDispatchService temporary;
        assert(temporary.start());
        survivor = std::move(temporary.submit(complete()).handle);
    }
    assert(survivor.status() == TaskStatus::Cancelled);
    TaskDispatchService independent;
    assert(independent.start());
    auto owned = independent.submit(complete());
    auto replacement = independent.submit(complete());
    auto oldId = owned.handle.id();
    owned.handle = std::move(replacement.handle);
    assert(independent.status(oldId) == TaskStatus::Cancelled);
}

void priorities() {
    TaskDispatchService manager;
    assert(manager.start());
    std::vector<int> order;
    auto repeated = [&](int id) {
        return task([&, id](const auto&) {
            order.push_back(id);
            return ExecutionResult::Yield;
        });
    };
    auto low = manager.submit(repeated(3), {.priority = Priority::Low});
    auto normal = manager.submit(repeated(2));
    auto high = manager.submit(repeated(0), {.priority = Priority::High});
    auto high2 = manager.submit(repeated(1), {.priority = Priority::High});
    for (int i = 0; i < 14; ++i) assert(manager.update(0, 1) == 1);
    assert((order == std::vector<int>{0, 1, 0, 1, 2, 2, 3, 0, 1, 0, 1, 2, 2, 3}));
}

void waitingAndTimeout() {
    TaskDispatchService manager;
    assert(manager.start());
    int calls = 0;
    auto waiting = manager.submit(
        task([&](const auto&) { return ++calls == 1 ? ExecutionResult::Wait : ExecutionResult::Complete; }),
        {.timeout = 100});
    auto other = manager.submit(complete());
    assert(manager.update(0, 10) == 2);
    assert(waiting.handle.status() == TaskStatus::Waiting && other.handle.status() == TaskStatus::Succeeded);
    assert(manager.update(50, 10) == 0);
    assert(manager.notify(waiting.handle.id()));
    assert(manager.update(50, 10) == 1);
    auto expired = manager.submit(task([](const auto&) { return ExecutionResult::Wait; }), {.timeout = 20});
    manager.update(50, 1);
    manager.update(70, 0);
    assert(expired.handle.status() == TaskStatus::TimedOut);
    assert(!manager.notify(expired.handle.id()));
    auto delayed = manager.submit(complete(), {.delay = 30, .timeout = 10});
    manager.update(80, 10);
    assert(delayed.handle.status() == TaskStatus::TimedOut);
}

void timingAndRetry() {
    TaskDispatchService manager(10, std::numeric_limits<Tick>::max() - 5);
    assert(manager.start());
    auto delayed = manager.submit(complete(), {.delay = 10});
    assert(manager.update(3, 10) == 0);
    assert(manager.update(4, 10) == 1);
    std::vector<std::uint32_t> attempts;
    ScheduleOptions options;
    options.retry = {.maxRetries = 3, .initialDelay = 10, .maxDelay = 15, .idempotent = true};
    auto retry = manager.submit(task([&](const auto& c) {
                                    attempts.push_back(c.attempt);
                                    return ExecutionResult::Retry;
                                }),
                                options);
    manager.update(4, 10);
    assert(retry.handle.status() == TaskStatus::RetryDelay);
    assert(manager.update(13, 10) == 0);
    manager.update(14, 10);
    manager.update(29, 10);
    manager.update(44, 10);
    assert((attempts == std::vector<std::uint32_t>{0, 1, 2, 3}));
    assert(retry.handle.status() == TaskStatus::Failed);
    options.retry.idempotent = false;
    assert(manager.submit(complete(), options).error == SubmitError::Invalid);
    auto permanent = manager.submit(task([](const auto&) { return ExecutionResult::Failed; }));
    manager.update(44, 1);
    assert(permanent.handle.status() == TaskStatus::Failed);
}

void jitterAndRetryTimeout() {
    TaskDispatchService manager;
    assert(manager.start());
    ScheduleOptions options;
    options.timeout = 15;
    options.retry = {.maxRetries = 5, .initialDelay = 10, .maxDelay = 30, .jitter = 5, .idempotent = true};
    int calls = 0;
    auto retry = manager.submit(task([&](const auto&) {
                                    ++calls;
                                    return ExecutionResult::Retry;
                                }),
                                options);
    manager.update(0, 1);
    const Tick expected = 10 + (retry.handle.id() * 1664525ULL + 1013904223ULL) % 6;
    assert(manager.update(expected - 1, 1) == 0);
    manager.update(expected, 1);
    assert(calls == 2);
    manager.update(15, 0);
    assert(retry.handle.status() == TaskStatus::TimedOut);
    assert(manager.update(100, 10) == 0);
}

void periodicAndReentrancy() {
    TaskDispatchService manager;
    assert(manager.start());
    std::vector<std::uint64_t> occurrences;
    auto periodic = manager.scheduleEvery(10, task([&](const auto& c) {
                                              occurrences.push_back(c.occurrence);
                                              return ExecutionResult::Complete;
                                          }),
                                          {.delay = 5});
    assert(manager.scheduleEvery(0, complete()).error == SubmitError::Invalid);
    assert(manager.update(4, 10) == 0);
    manager.update(5, 10);
    manager.update(100, 10);
    assert((occurrences == std::vector<std::uint64_t>{0, 1}));
    assert(manager.update(104, 10) == 0);
    manager.update(105, 10);
    assert(occurrences.size() == 3);
    periodic.handle.cancel();
    TaskId id = 0;
    auto self = manager.submit(task([&](const auto&) {
        assert(manager.update(1000, 10) == 0);
        assert(manager.status(id) == TaskStatus::Running);
        manager.cancel(id);
        auto nested = manager.submit(complete());
        return ExecutionResult::Complete;
    }));
    id = self.handle.id();
    manager.update(105, 10);
    assert(self.handle.status() == TaskStatus::Cancelled);
    manager.stop();
    assert(!manager.isRunning() && manager.update(106, 10) == 0);
    assert(manager.submit(complete()).error == SubmitError::Stopped);
}

void restartLifecycle() {
    TaskDispatchService service;
    assert(!service.isRunning());
    assert(service.submit(complete()).error == SubmitError::Stopped);
    service.stop();
    assert(service.start());
    auto done = service.submit(complete());
    service.update(0, 1);
    auto pending = service.submit(complete(), {.delay = 100});
    const auto oldId = pending.handle.id();
    assert(service.start());
    assert(service.size() == 1);
    service.stop();
    assert(pending.handle.status() == TaskStatus::Cancelled);
    assert(done.handle.status() == TaskStatus::Succeeded);
    assert(!service.status(oldId));
    assert(service.update(10000, 0) == 0);
    assert(service.start());
    auto next = service.submit(complete(), {.delay = 10});
    assert(next.handle.id() > oldId);
    assert(!service.cancel(oldId) && !service.notify(oldId));
    assert(service.update(10009, 1) == 0);
    assert(service.update(10010, 1) == 1);
    assert(next.handle.status() == TaskStatus::Succeeded);
}

}  // namespace

int main() {
    restartLifecycle();
    queueAndLifetime();
    priorities();
    waitingAndTimeout();
    timingAndRetry();
    jitterAndRetryTimeout();
    periodicAndReentrancy();
    std::cout << "TaskDispatchService deterministic tests passed\n";
}
