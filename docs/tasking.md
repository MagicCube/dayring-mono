# Task Scheduling

`src/platform/tasking/Task.h` defines `Task`, `ExecutionResult`, `TaskContext`, scheduling options, and the move-only RAII `TaskHandle`. `src/platform/tasking/services/TaskDispatchService.h/.cpp` implements an independently constructible, single-threaded C++20 scheduler using only the standard library. Shell's ServiceManager owns the production instance; no Arduino, native adapter, RTOS, BLE, or storage dependency enters the core.

## Small-device scope

The default capacity is 32 active jobs, including delayed, waiting, periodic, and retrying jobs. A reserved vector and linear scans keep the implementation small; scheduling uses no threads, locks, timer objects, or per-step allocations. Task objects, queue entries, handle state, and optional dedupe strings allocate when submitted. Caller-retained terminal handles retain only their small status record, not the task. This is bounded queue management, not a fixed-allocation arena.

High/normal/low priorities receive 4/2/1 slots per cycle, skipping unavailable classes. Each class rotates FIFO after every step. The cycle persists across updates, including one-step budgets, so continuously runnable low-priority work cannot starve. Priority controls cooperative selection, not preemption.

## Ownership and results

- Submit a `unique_ptr<Task>` with `submit`, or use `scheduleEvery(interval, task, options)`. Submission takes ownership even when rejected. Inspect `Submission::error`: full, duplicate, invalid options, and stopped are distinct errors.
- Keep the returned handle alive. Its destruction or replacement cancels active work. Handles remain safe after manager destruction and retain terminal status; destroying the manager cancels unfinished handles.
- A nonempty dedupe key is unique among active tasks, including periodic work. A duplicate is rejected rather than returning another cancelling owner of the same task.
- `cancel(id)` is idempotent; cancellation wins over a result returned by the current step. It cannot interrupt synchronous code. Resources are released on the next collection (submission/update), or manager destruction.
- `status(id)` queries entries still held by the manager. Completed entries are collected; use `handle.status()` for durable final state. There is no unbounded completion history.
- Instances begin stopped. `start()` enables submissions; `stop()` rejects submissions with `SubmitError::Stopped`, cancels unfinished handles, and releases tasks synchronously. Both are idempotent and must run outside task execution/callbacks. Restart has an empty queue and preserves task IDs; old handles remain safe.
- Firmware preparation stops all services in reverse order and prevents Shell from restarting them. Future services must complete safe writes before returning from stop; asynchronous draining is not implemented.

Applications/controllers retain page handles and cancel them in `onLeave`; global services retain background handles. Tasks own their inputs and must not capture raw application/controller pointers. Register global jobs at boot, not `onCreate`, which route discovery also calls. All scheduler APIs, handle destruction, task destruction, and event notifications run on the owning loop. Destructors must not reenter the scheduler. Task execution must not throw.

## Time and execution

`Tick` is an unsigned 32-bit monotonic millisecond sample. Construct with the current tick (`initialNow`, default zero), call `start()`, then call `update(now, budget)`. Before restarting after an idle period, sample time with `update(now, 0)`; Shell passes current time through ServiceManager in `startServices()`. Advance time before submitting work when the last sample is stale: delays and total timeouts are relative to the last supplied tick. Samples must advance monotonically modulo 2^32 with less than one full wrap between updates; wall-clock adjustments and backwards samples are not supported. Internal elapsed time is extended to 64 bits.

`budget` limits calls to `execute`, not wall time. Zero budget still updates time and expires jobs. Each execute call must perform bounded, nonblocking work; the scheduler cannot enforce a wall-time limit on a blocking step. Reentrant `update` calls do nothing; submission and cancellation from an executing task are supported.

| Result | Behavior |
| --- | --- |
| `Complete` | Succeed, or schedule the next periodic occurrence |
| `Failed` | Permanent terminal failure |
| `Retry` | Retry transient failure only under an explicitly idempotent retry policy |
| `Wait` | Suspend until caller invokes `notify(id)` |
| `Yield` | Remain runnable and rotate behind peers |

`notify` only affects waiting jobs; early notifications are not latched, and late notifications cannot revive terminal jobs. Waiting does not consume retries. `timeout` bounds the entire submitted lifetime, including initial delay, retries, waits, and periodic occurrences; zero disables it. Expiration wins at the exact deadline before execution, producing `TimedOut`.

Retries use capped exponential delay plus deterministic per-task jitter, capped by `maxDelay`. `maxRetries` excludes the initial attempt. `TaskContext::attempt` starts at zero and increments on retry; a task must reset its operation state when that changes. A zero retry count makes `Retry` terminal.

Periodic tasks first run after `options.delay` (zero means immediately), then follow a fixed cadence. Missed ticks coalesce into one execution; no overlapping occurrence or backlog is created. On completion the next due time is the first cadence point strictly after now. `TaskContext::occurrence` increments after successful completion and resets the attempt counter. The same task object is reused and must initialize new occurrence state itself. Permanent failure or exhausted retries terminates the schedule.

## Verification

`make test-task-dispatch-service` compiles directly with the host C++20 compiler, `-fno-exceptions`, and only `-Isrc`; it needs no PlatformIO, SDK, or native stubs. Tests advance virtual ticks without sleeping. Shell ownership/lock/switch checks run under `make test-shell-facade`, firmware shutdown under `make test-shell`, and preview build coverage under `make test-preview`.
