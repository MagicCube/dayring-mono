#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "../../runtime/services/Service.h"
#include "../Task.h"

namespace platform::tasking {

// Single-threaded. All callbacks and notifications belong on the owning loop.
class TaskDispatchService final : public runtime::Service {
   public:
    explicit TaskDispatchService(std::size_t capacity = 32, Tick initialNow = 0);
    ~TaskDispatchService() override;
    TaskDispatchService(const TaskDispatchService&) = delete;
    TaskDispatchService& operator=(const TaskDispatchService&) = delete;
    TaskDispatchService(TaskDispatchService&&) = delete;
    TaskDispatchService& operator=(TaskDispatchService&&) = delete;
    [[nodiscard]] Submission submit(std::unique_ptr<Task> task, ScheduleOptions options = {});
    [[nodiscard]] Submission scheduleEvery(Tick interval, std::unique_ptr<Task> task, ScheduleOptions options = {});
    bool cancel(TaskId id);
    bool notify(TaskId id);
    [[nodiscard]] std::optional<TaskStatus> status(TaskId id) const;
    [[nodiscard]] std::size_t size() const;
    // Budget counts execute calls, not elapsed wall time. Tick may wrap once between updates.
    std::size_t update(Tick now, std::size_t budget);
    void update(Tick now) override;
    [[nodiscard]] bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const;

   private:
    struct Entry;
    [[nodiscard]] Submission _submit(std::unique_ptr<Task> task, ScheduleOptions options, Tick interval);
    void _collect();
    void _expire();
    [[nodiscard]] std::shared_ptr<Entry> _next();
    void _finish(Entry& entry, ExecutionResult result);
    [[nodiscard]] Tick _retryDelay(const Entry& entry) const;
    std::vector<std::shared_ptr<Entry>> _entries;
    std::size_t _capacity;
    Tick _now;
    std::uint64_t _elapsed = 0;
    TaskId _nextId = 1;
    std::uint64_t _sequence = 0;
    std::size_t _turn = 0;
    bool _updating = false;
    bool _running = false;
};

}  // namespace platform::tasking
