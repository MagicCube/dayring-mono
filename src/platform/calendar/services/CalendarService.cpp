#include "CalendarService.h"

#include <algorithm>

namespace platform::calendar {

CalendarService::CalendarService(rpc::RPCService& rpc, Clock clock, std::unique_ptr<CalendarStorage> storage)
    : _rpc(rpc), _clock(std::move(clock)), _storage(std::move(storage)) {
}

CalendarService::~CalendarService() {
    stop();
    _changes.clear();
}

bool CalendarService::start() {
    if (_running) return true;
    if (!_rpc.registerHandler(9, [this](std::span<const uint8_t> payload) {
            if (!payload.empty()) return rpc::Reply{.error = rpc::Error::InvalidPayload};
            synchronize();
            return rpc::Reply{};
        }))
        return false;
    if (!_rpc.registerHandler(11, [this](std::span<const uint8_t> payload) {
            if (!payload.empty()) return rpc::Reply{.error = rpc::Error::InvalidPayload};
            rpc::Reply reply{.payload = std::vector<uint8_t>(12)};
            reply.payload[0] = 1;
            reply.payload[1] = static_cast<uint8_t>(_syncState);
            reply.payload[2] = static_cast<uint8_t>(_persistenceState);
            reply.payload[3] = static_cast<uint8_t>(_failure);
            const size_t count = _snapshot ? _snapshot->events.size() : 0;
            reply.payload[4] = static_cast<uint8_t>(count);
            reply.payload[5] = static_cast<uint8_t>(count >> 8);
            reply.payload[6] = static_cast<uint8_t>(_upcoming.size());
            reply.payload[7] = static_cast<uint8_t>(_upcoming.size() >> 8);
            reply.payload[8] = hasCurrentCoverage();
            reply.payload[9] = hasUsableTime();
            return reply;
        })) {
        _rpc.removeHandler(9);
        return false;
    }
    _running = true;
    _session = 0;
    _dirty = true;
    _delay = 0;
    _sample = _clock();
    if (!_snapshot && _storage) {
        if (auto json = _storage->load()) {
            if (auto parsed = parseSnapshot(*json)) {
                _snapshot = std::make_shared<Snapshot>(std::move(*parsed));
                _json = std::move(*json);
                _persistenceState = PersistenceState::Saved;
                _refreshUpcoming(UpcomingChange::Reason::Restored);
            }
        }
    }
    _refreshUpcoming(UpcomingChange::Reason::Time);
    return true;
}

void CalendarService::stop() {
    if (!_running) return;
    _running = false;
    _rpc.removeHandler(9);
    _rpc.removeHandler(11);
    _resetTransfer();
    _session = 0;
    _syncState = SyncState::Waiting;
}

void CalendarService::_resetTransfer() {
    ++_epoch;
    if (_request) _rpc.cancel(_request);
    _request = 0;
    _phase = Phase::Idle;
    _hasReply = false;
    _reply.clear();
    _pendingJson.clear();
    _manifest = {};
}

void CalendarService::synchronize() {
    _dirty = true;
    _delay = 0;
}

void CalendarService::update(uint32_t now) {
    if (!_running) return;
    const auto current = _clock();
    if (current != _sample) {
        const bool dayChanged = current.localSeconds && _sample.localSeconds &&
                                *current.localSeconds / 86400 != *_sample.localSeconds / 86400;
        if (dayChanged || current.timeZone != _sample.timeZone || current.utcOffsetSeconds != _sample.utcOffsetSeconds)
            synchronize();
        _sample = current;
        _refreshUpcoming(UpcomingChange::Reason::Time);
    }
    const auto session = _rpc.session();
    if (session != _session) {
        _resetTransfer();
        _session = session;
        _syncState = SyncState::Waiting;
        synchronize();
    }
    _sync(now);
    _persist(now);
}

bool CalendarService::isRunning() const {
    return _running;
}

bool CalendarService::hasSnapshot() const {
    return bool(_snapshot);
}

std::shared_ptr<const Snapshot> CalendarService::snapshot() const {
    return _snapshot;
}

CalendarService::SyncState CalendarService::syncState() const {
    return _syncState;
}

CalendarService::PersistenceState CalendarService::persistenceState() const {
    return _persistenceState;
}

CalendarService::Failure CalendarService::failure() const {
    return _failure;
}

bool CalendarService::hasUsableTime() const {
    return _utcNow().has_value();
}

bool CalendarService::hasCurrentCoverage() const {
    return _snapshot && _sample.localSeconds && hasUsableTime() &&
           *_sample.localSeconds / 86400 == _snapshot->windowStart.localSeconds() / 86400;
}

std::optional<int64_t> CalendarService::_utcNow() const {
    if (!_snapshot || !_sample.localSeconds) return std::nullopt;
    if (!_sample.timeZone.empty() && _sample.timeZone != _snapshot->timeZone) return std::nullopt;
    const auto local = *_sample.localSeconds;
    if (local < _snapshot->windowStart.localSeconds() || local >= _snapshot->windowEndExclusive.localSeconds())
        return std::nullopt;
    if (_sample.utcOffsetSeconds) return local - *_sample.utcOffsetSeconds;
    // Restored RTC uses local wall time. Only infer an offset for a window with no transition.
    const auto offset = _snapshot->generatedAt.offsetSeconds;
    if (_snapshot->windowStart.offsetSeconds != offset || _snapshot->windowEndExclusive.offsetSeconds != offset)
        return std::nullopt;
    return local - offset;
}

void CalendarService::_refreshUpcoming(UpcomingChange::Reason reason, const Snapshot* previous) {
    std::vector<Selection> selected;
    if (const auto now = _utcNow()) {
        for (size_t i = 0; i < _snapshot->events.size(); ++i) {
            const auto& event = _snapshot->events[i];
            const bool ongoing = event.start.utcSeconds <= *now && *now < event.end.utcSeconds;
            if (event.end.utcSeconds > *now ||
                (event.start.utcSeconds == event.end.utcSeconds && event.start.utcSeconds >= *now))
                selected.push_back({i, ongoing});
        }
    }
    if (!previous) previous = _snapshot.get();
    bool changed = selected.size() != _upcoming.size();
    for (size_t i = 0; !changed && i < selected.size(); ++i) {
        changed = !previous || selected[i].isOngoing != _upcoming[i].isOngoing ||
                  _snapshot->events[selected[i].index] != previous->events[_upcoming[i].index];
    }
    _upcoming = std::move(selected);
    if (changed) _changes.publish({++_revision, reason});
}

std::vector<UpcomingEvent> CalendarService::upcoming(size_t maxCount) const {
    std::vector<UpcomingEvent> result;
    const size_t count = std::min(maxCount, _upcoming.size());
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const auto& item = _upcoming[i];
        result.push_back({_snapshot->events[item.index], item.isOngoing});
    }
    return result;
}

UpcomingSubscription CalendarService::subscribeUpcoming(std::function<void(const UpcomingChange&)> callback) {
    return _changes.subscribe(std::move(callback));
}

void CalendarService::_persist(uint32_t now) {
    if (!_savePending || (_saveAttempted && now - _saveAt < 30000)) return;
    _saveAttempted = true;
    _saveAt = now;
    if (_storage && _storage->save(_json)) {
        _savePending = false;
        _persistenceState = PersistenceState::Saved;
    } else {
        _persistenceState = PersistenceState::Failed;
    }
}

}  // namespace platform::calendar
