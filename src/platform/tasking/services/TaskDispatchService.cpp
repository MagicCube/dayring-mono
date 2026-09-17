#include "TaskDispatchService.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

namespace platform::tasking {
namespace {

bool terminal(TaskStatus status) {
    return status == TaskStatus::Succeeded || status == TaskStatus::Failed || status == TaskStatus::Cancelled ||
           status == TaskStatus::TimedOut;
}

}  // namespace

TaskHandle::TaskHandle(std::shared_ptr<detail::TaskState> state) : _state(std::move(state)) {
}

TaskHandle::~TaskHandle() {
    cancel();
}

TaskHandle::TaskHandle(TaskHandle&& other) noexcept : _state(std::move(other._state)) {
}

TaskHandle& TaskHandle::operator=(TaskHandle&& other) noexcept {
    if (this != &other) {
        cancel();
        _state = std::move(other._state);
    }
    return *this;
}

TaskHandle::operator bool() const {
    return static_cast<bool>(_state);
}

TaskId TaskHandle::id() const {
    return _state ? _state->id : 0;
}

TaskStatus TaskHandle::status() const {
    return _state ? _state->status : TaskStatus::Cancelled;
}

bool TaskHandle::cancel() {
    if (!_state || terminal(_state->status)) return false;
    _state->status = TaskStatus::Cancelled;
    return true;
}

struct TaskDispatchService::Entry {
    std::shared_ptr<detail::TaskState> state;
    std::unique_ptr<Task> task;
    ScheduleOptions options;
    Tick interval;
    std::uint64_t submitted;
    std::uint64_t due;
    std::uint64_t cadence;
    std::uint64_t sequence;
    std::uint32_t attempt = 0;
    std::uint64_t occurrence = 0;
};

TaskDispatchService::TaskDispatchService(std::size_t capacity, Tick initialNow)
    : _capacity(capacity), _now(initialNow) {
    _entries.reserve(capacity);
}

TaskDispatchService::~TaskDispatchService() {
    stop();
}

Submission TaskDispatchService::submit(std::unique_ptr<Task> task, ScheduleOptions options) {
    return _submit(std::move(task), std::move(options), 0);
}

Submission TaskDispatchService::scheduleEvery(Tick interval, std::unique_ptr<Task> task, ScheduleOptions options) {
    if (!interval) return {{}, SubmitError::Invalid};
    return _submit(std::move(task), std::move(options), interval);
}

Submission TaskDispatchService::_submit(std::unique_ptr<Task> task, ScheduleOptions options, Tick interval) {
    if (!_running) return {{}, SubmitError::Stopped};
    if (!task || static_cast<unsigned>(options.priority) > static_cast<unsigned>(Priority::Low) ||
        (options.retry.maxRetries && (!options.retry.idempotent || !options.retry.initialDelay ||
                                      options.retry.initialDelay > options.retry.maxDelay)))
        return {{}, SubmitError::Invalid};
    _collect();
    for (const auto& entry : _entries) {
        if (!options.dedupeKey.empty() && options.dedupeKey == entry->options.dedupeKey)
            return {{}, SubmitError::Duplicate};
    }
    if (size() >= _capacity || !_nextId) return {{}, SubmitError::Full};
    auto state = std::make_shared<detail::TaskState>(detail::TaskState{_nextId++});
    const auto due = _elapsed + options.delay;
    _entries.push_back(std::make_shared<Entry>(
        Entry{state, std::move(task), std::move(options), interval, _elapsed, due, due, _sequence++}));
    return {TaskHandle{std::move(state)}, SubmitError::None};
}

bool TaskDispatchService::cancel(TaskId id) {
    for (auto& entry : _entries) {
        if (entry->state->id == id && !terminal(entry->state->status)) {
            entry->state->status = TaskStatus::Cancelled;
            return true;
        }
    }
    return false;
}

bool TaskDispatchService::notify(TaskId id) {
    for (auto& entry : _entries) {
        if (entry->state->id == id && entry->state->status == TaskStatus::Waiting) {
            entry->state->status = TaskStatus::Ready;
            entry->due = _elapsed;
            entry->sequence = _sequence++;
            return true;
        }
    }
    return false;
}

std::optional<TaskStatus> TaskDispatchService::status(TaskId id) const {
    for (const auto& entry : _entries)
        if (entry->state->id == id) return entry->state->status;
    return std::nullopt;
}

std::size_t TaskDispatchService::size() const {
    return std::count_if(_entries.begin(), _entries.end(),
                         [](const auto& entry) { return !terminal(entry->state->status); });
}

void TaskDispatchService::_collect() {
    std::erase_if(_entries, [](const auto& entry) { return terminal(entry->state->status); });
}

void TaskDispatchService::_expire() {
    for (auto& entry : _entries) {
        if (!terminal(entry->state->status) && entry->options.timeout &&
            _elapsed - entry->submitted >= entry->options.timeout)
            entry->state->status = TaskStatus::TimedOut;
    }
}

std::shared_ptr<TaskDispatchService::Entry> TaskDispatchService::_next() {
    constexpr std::array priorities{Priority::High,   Priority::High,   Priority::High, Priority::High,
                                    Priority::Normal, Priority::Normal, Priority::Low};
    for (std::size_t scanned = 0; scanned < priorities.size(); ++scanned) {
        const auto priority = priorities[_turn];
        _turn = (_turn + 1) % priorities.size();
        std::shared_ptr<Entry> selected;
        for (const auto& entry : _entries) {
            const auto state = entry->state->status;
            if ((state == TaskStatus::Ready || state == TaskStatus::RetryDelay) && entry->due <= _elapsed &&
                entry->options.priority == priority && (!selected || entry->sequence < selected->sequence))
                selected = entry;
        }
        if (selected) return selected;
    }
    return {};
}

Tick TaskDispatchService::_retryDelay(const Entry& entry) const {
    const auto& retry = entry.options.retry;
    std::uint64_t delay = retry.initialDelay;
    for (std::uint32_t i = 1; i < entry.attempt && delay < retry.maxDelay; ++i)
        delay = std::min<std::uint64_t>(delay * 2, retry.maxDelay);
    // Stable per-task jitter makes virtual-time runs reproducible.
    const auto noise =
        (entry.state->id * 1664525ULL + entry.attempt * 1013904223ULL) % (std::uint64_t{retry.jitter} + 1);
    return static_cast<Tick>(std::min<std::uint64_t>(delay + noise, retry.maxDelay));
}

void TaskDispatchService::_finish(Entry& entry, ExecutionResult result) {
    auto& state = entry.state->status;
    if (terminal(state)) return;
    entry.sequence = _sequence++;
    switch (result) {
        case ExecutionResult::Wait:
            state = TaskStatus::Waiting;
            break;
        case ExecutionResult::Yield:
            state = TaskStatus::Ready;
            break;
        case ExecutionResult::Failed:
            state = TaskStatus::Failed;
            break;
        case ExecutionResult::Retry:
            if (entry.attempt >= entry.options.retry.maxRetries) {
                state = TaskStatus::Failed;
                break;
            }
            ++entry.attempt;
            entry.due = _elapsed + _retryDelay(entry);
            state = TaskStatus::RetryDelay;
            break;
        case ExecutionResult::Complete:
            if (!entry.interval) {
                state = TaskStatus::Succeeded;
                break;
            }
            entry.cadence += ((_elapsed - entry.cadence) / entry.interval + 1) * entry.interval;
            entry.due = entry.cadence;
            entry.attempt = 0;
            ++entry.occurrence;
            state = TaskStatus::Ready;
            break;
    }
}

std::size_t TaskDispatchService::update(Tick now, std::size_t budget) {
    if (_updating) return 0;
    _elapsed += static_cast<Tick>(now - _now);
    _now = now;
    _expire();
    if (!_running) {
        _collect();
        return 0;
    }
    _updating = true;

    struct Reset {
        bool& value;

        ~Reset() {
            value = false;
        }
    } reset{_updating};

    std::size_t executed = 0;
    while (executed < budget && _running) {
        auto entry = _next();
        if (!entry) break;
        entry->state->status = TaskStatus::Running;
        const auto result = entry->task->execute({_now, entry->attempt, entry->occurrence});
        ++executed;
        _finish(*entry, result);
    }
    _collect();
    return executed;
}

void TaskDispatchService::update(Tick now) {
    update(now, 8);
}

bool TaskDispatchService::start() {
    assert(!_updating);
    _running = true;
    return true;
}

void TaskDispatchService::stop() {
    assert(!_updating);
    _running = false;
    for (auto& entry : _entries) {
        if (!terminal(entry->state->status)) entry->state->status = TaskStatus::Cancelled;
    }
    _entries.clear();
    _turn = 0;
}

bool TaskDispatchService::isRunning() const {
    return _running;
}

}  // namespace platform::tasking
