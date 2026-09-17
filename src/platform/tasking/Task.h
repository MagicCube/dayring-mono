#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace platform::tasking {

using TaskId = std::uint64_t;
using Tick = std::uint32_t;
enum class Priority { High, Normal, Low };
enum class TaskStatus { Ready, Running, Waiting, RetryDelay, Succeeded, Failed, Cancelled, TimedOut };
enum class ExecutionResult { Complete, Failed, Retry, Wait, Yield };

struct TaskContext {
    Tick now;
    std::uint32_t attempt;
    std::uint64_t occurrence;
};

class Task {
   public:
    virtual ~Task() = default;
    // Each call must be bounded and nonblocking. A new attempt/occurrence starts fresh work.
    virtual ExecutionResult execute(const TaskContext& context) = 0;
};

struct RetryPolicy {
    std::uint32_t maxRetries = 0;
    Tick initialDelay = 100;
    Tick maxDelay = 30000;
    Tick jitter = 0;
    bool idempotent = false;
};

struct ScheduleOptions {
    Priority priority = Priority::Normal;
    std::string dedupeKey;
    Tick delay = 0;
    // Total lifetime from submission, including delays and waits; zero disables timeout.
    Tick timeout = 0;
    RetryPolicy retry;
};

namespace detail {

struct TaskState {
    TaskId id;
    TaskStatus status = TaskStatus::Ready;
};

}  // namespace detail

class TaskHandle {
   public:
    TaskHandle() = default;
    ~TaskHandle();
    TaskHandle(TaskHandle&& other) noexcept;
    TaskHandle& operator=(TaskHandle&& other) noexcept;
    TaskHandle(const TaskHandle&) = delete;
    TaskHandle& operator=(const TaskHandle&) = delete;
    [[nodiscard]] explicit operator bool() const;
    [[nodiscard]] TaskId id() const;
    [[nodiscard]] TaskStatus status() const;
    bool cancel();

   private:
    friend class TaskDispatchService;
    explicit TaskHandle(std::shared_ptr<detail::TaskState> state);
    std::shared_ptr<detail::TaskState> _state;
};

enum class SubmitError { None, Full, Duplicate, Invalid, Stopped };

struct Submission {
    TaskHandle handle;
    SubmitError error = SubmitError::None;
};

}  // namespace platform::tasking
