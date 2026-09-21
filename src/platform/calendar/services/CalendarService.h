#pragma once

#include <functional>
#include <limits>
#include <memory>

#include "../../rpc/services/RPCService.h"
#include "../Calendar.h"
#include "../CalendarClock.h"
#include "../CalendarStorage.h"
#include "../UpcomingChanges.h"

namespace platform::calendar {

struct UpcomingEvent {
    Event event;
    bool isOngoing = false;
};

class CalendarService final : public runtime::Service {
   public:
    using Clock = std::function<ClockSample()>;
    enum class SyncState { Waiting, Syncing, Synchronized, Failed };
    enum class PersistenceState { Unavailable, Saved, Pending, Failed };
    enum class Failure { None, Transport, InvalidSnapshot, Capacity, TimeContext };

    CalendarService(rpc::RPCService& rpc, Clock clock,
                    std::unique_ptr<CalendarStorage> storage = makeCalendarStorage());
    ~CalendarService() override;
    bool start() override;
    void stop() override;
    void update(uint32_t now) override;
    void synchronize();
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool hasSnapshot() const;
    [[nodiscard]] bool hasUsableTime() const;
    [[nodiscard]] bool hasCurrentCoverage() const;
    [[nodiscard]] SyncState syncState() const;
    [[nodiscard]] PersistenceState persistenceState() const;
    [[nodiscard]] Failure failure() const;
    [[nodiscard]] std::shared_ptr<const Snapshot> snapshot() const;
    [[nodiscard]] std::vector<UpcomingEvent> upcoming(size_t maxCount = std::numeric_limits<size_t>::max()) const;
    [[nodiscard]] UpcomingSubscription subscribeUpcoming(std::function<void(const UpcomingChange&)> callback);

   private:
    enum class Phase { Idle, Begin, Read };

    struct Selection {
        size_t index;
        bool isOngoing;
    };

    void _resetTransfer();
    void _sync(uint32_t now);
    void _requestPage(uint32_t now);
    void _consumeReply(uint32_t now);
    void _commit(uint32_t now);
    void _fail(Failure failure, uint32_t now, uint32_t delay = 30000);
    void _persist(uint32_t now);
    void _refreshUpcoming(UpcomingChange::Reason reason, const Snapshot* previous = nullptr);
    [[nodiscard]] std::optional<int64_t> _utcNow() const;

    rpc::RPCService& _rpc;
    Clock _clock;
    std::unique_ptr<CalendarStorage> _storage;
    std::shared_ptr<const Snapshot> _snapshot;
    std::vector<Selection> _upcoming;
    UpcomingChanges _changes;
    ClockSample _sample;
    std::string _json;
    std::string _pendingJson;
    Manifest _manifest;
    std::vector<uint8_t> _reply;
    rpc::Error _replyError = rpc::Error::None;
    Phase _phase = Phase::Idle;
    SyncState _syncState = SyncState::Waiting;
    PersistenceState _persistenceState = PersistenceState::Unavailable;
    Failure _failure = Failure::None;
    uint64_t _revision = 0;
    uint32_t _session = 0;
    uint32_t _epoch = 0;
    uint32_t _delayAt = 0;
    uint32_t _delay = 0;
    uint32_t _beganAt = 0;
    uint32_t _saveAt = 0;
    uint16_t _request = 0;
    bool _hasReply = false;
    bool _dirty = true;
    bool _savePending = false;
    bool _saveAttempted = false;
    bool _running = false;
};

}  // namespace platform::calendar
