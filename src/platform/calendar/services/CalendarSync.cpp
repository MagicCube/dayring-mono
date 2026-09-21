#include <algorithm>

#include "CalendarService.h"

namespace platform::calendar {
namespace {

std::string_view text(std::span<const uint8_t> bytes) {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}  // namespace

void CalendarService::_sync(uint32_t now) {
    if (!_session) return;
    if (_hasReply) {
        _hasReply = false;
        _consumeReply(now);
        _reply.clear();
    }
    if (!_running || !_session || _request || now - _delayAt < _delay) return;
    if (_phase == Phase::Idle) {
        if (!_dirty) return;
        _dirty = false;
        _phase = Phase::Begin;
        _beganAt = now;
        _syncState = SyncState::Syncing;
        _failure = Failure::None;
    }
    if ((_phase == Phase::Begin && now - _beganAt >= 120000U) || now - _beganAt >= 20U * 60U * 1000U) {
        _fail(Failure::Transport, now);
        return;
    }
    _requestPage(now);
}

void CalendarService::_requestPage(uint32_t now) {
    const auto json = _phase == Phase::Read ? readRequest(_manifest, _pendingJson.size()) : std::string{};
    const auto epoch = _epoch;
    const uint16_t method = _phase == Phase::Read ? 10 : 8;
    _request = _rpc.request(
        method, {reinterpret_cast<const uint8_t*>(json.data()), json.size()},
        [this, epoch](rpc::Error error, std::span<const uint8_t> bytes) {
            if (!_running || epoch != _epoch) return;
            _request = 0;
            _replyError = error;
            _reply.assign(bytes.begin(), bytes.end());
            _hasReply = true;
        },
        now);
    if (!_request) _fail(Failure::Transport, now);
}

void CalendarService::_consumeReply(uint32_t now) {
    if (_replyError == rpc::Error::Busy) {
        _delayAt = now;
        _delay = 1000;
        return;
    }
    if (_replyError != rpc::Error::None) {
        _fail(Failure::Transport, now);
        return;
    }
    _delay = 0;
    if (_phase == Phase::Begin) {
        auto manifest = parseManifest(text(_reply));
        if (!manifest) {
            _fail(Failure::InvalidSnapshot, now);
            return;
        }
        _manifest = std::move(*manifest);
        _pendingJson.clear();
        _pendingJson.reserve(_manifest.byteLength);
        _phase = Phase::Read;
        return;
    }
    const size_t expected = std::min(_manifest.chunkBytes, _manifest.byteLength - _pendingJson.size());
    if (_reply.size() != expected) {
        _fail(Failure::InvalidSnapshot, now);
        return;
    }
    _pendingJson.append(text(_reply));
    if (_pendingJson.size() == _manifest.byteLength) _commit(now);
}

void CalendarService::_commit(uint32_t now) {
    auto parsed = parseSnapshot(_pendingJson);
    if (!parsed) {
        _fail(Failure::InvalidSnapshot, now);
        return;
    }
    if ((!_sample.timeZone.empty() && parsed->timeZone != _sample.timeZone) ||
        (_sample.localSeconds && *_sample.localSeconds / 86400 != parsed->windowStart.localSeconds() / 86400)) {
        _fail(Failure::TimeContext, now, 1000);
        return;
    }
    const auto previous = _snapshot;
    _snapshot = std::make_shared<Snapshot>(std::move(*parsed));
    const bool contentChanged = !previous || previous->events != _snapshot->events ||
                                previous->timeZone != _snapshot->timeZone ||
                                previous->windowStart != _snapshot->windowStart ||
                                previous->windowEndExclusive != _snapshot->windowEndExclusive ||
                                previous->generatedAt.offsetSeconds != _snapshot->generatedAt.offsetSeconds;
    if (contentChanged) {
        _json = std::move(_pendingJson);
        _savePending = true;
        _saveAttempted = false;
        _persistenceState = PersistenceState::Pending;
    }
    _pendingJson.clear();
    _phase = Phase::Idle;
    _syncState = SyncState::Synchronized;
    _failure = Failure::None;
    _refreshUpcoming(UpcomingChange::Reason::Snapshot, previous.get());
}

void CalendarService::_fail(Failure failure, uint32_t now, uint32_t delay) {
    _resetTransfer();
    _failure = failure;
    _syncState = SyncState::Failed;
    _dirty = true;
    _delayAt = now;
    _delay = delay;
}

}  // namespace platform::calendar
