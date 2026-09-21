#include "TimeService.h"

#include <Arduino.h>

#include "../../hal/RtcClock.h"
#include "../ClockSample.h"

namespace platform::time {

bool TimeService::start() {
    if (_running) return true;
    if (_rpc && !_rpc->registerHandler(rpc::clockChangedMethod, [this](std::span<const uint8_t> data) {
            if (!data.empty()) return rpc::Reply{.error = rpc::Error::InvalidPayload};
            _resyncRequested = true;
            return rpc::Reply{};
        }))
        return false;
    if (_rpc && !_rpc->registerHandler(rpc::clockStatusMethod, [this](std::span<const uint8_t> data) {
            if (!data.empty()) return rpc::Reply{.error = rpc::Error::InvalidPayload};
            rpc::Reply reply{.size = 12};
            reply.payload[0] = static_cast<uint8_t>(_syncState);
            reply.payload[1] = _time.year;
            reply.payload[2] = _time.year >> 8;
            reply.payload[3] = _time.month;
            reply.payload[4] = _time.day;
            reply.payload[5] = _time.hour;
            reply.payload[6] = _time.minute;
            reply.payload[7] = _time.second;
            for (int i = 0; i < 4; ++i) reply.payload[8 + i] = static_cast<uint32_t>(_utcOffset) >> (8 * i);
            return reply;
        })) {
        _rpc->removeHandler(rpc::clockChangedMethod);
        return false;
    }
    _resyncRequested = false;
    _sample(static_cast<uint32_t>(millis()), true);
    _session = 0;
    _attempted = false;
    _synchronized = false;
    _syncState = SyncState::Waiting;
    _running = true;
    return true;
}

void TimeService::stop() {
    if (!_running) return;
    _running = false;
    if (_rpc) {
        _rpc->removeHandler(rpc::clockChangedMethod);
        _rpc->removeHandler(rpc::clockStatusMethod);
    }
    if (_rpc && _request) _rpc->cancel(_request);
    _request = 0;
}

void TimeService::update(uint32_t now) {
    if (!_running) return;
    if (now - _sampledAt >= 1000U) _sample(now, false);
    _sync(now);
}

void TimeService::_sync(uint32_t now) {
    if (!_rpc) return;
    const auto session = _rpc->session();
    if (_session != session) {
        if (_request) _rpc->cancel(_request);
        _session = session;
        _request = 0;
        _attempted = false;
        _synchronized = false;
        _syncState = SyncState::Waiting;
    }
    if (!session || _request) return;
    if (_resyncRequested) {
        _resyncRequested = false;
        _attempted = false;
        _synchronized = false;
    }
    if (_synchronized && now - _lastSuccess < 8U * 60U * 60U * 1000U) return;
    if (_attempted && now - _lastAttempt < 30000U) return;
    _lastAttempt = now;
    _attempted = true;
    _syncState = SyncState::Pending;
    _request = _rpc->request(
        rpc::clockGetMethod, {},
        [this](rpc::Error error, std::span<const uint8_t> bytes) {
            _request = 0;
            if (!_running) return;
            if (error != rpc::Error::None || !decodeClockSample(bytes)) {
                _syncState = SyncState::Failed;
                return;
            }
            std::copy(bytes.begin(), bytes.end(), _pendingSample.begin());
            _pendingZone.clear();
            _fetchTimeZone();
        },
        now);
    if (!_request) _syncState = SyncState::Failed;
}

void TimeService::_fetchTimeZone() {
    const std::array<uint8_t, 1> offset{static_cast<uint8_t>(_pendingZone.size())};
    _request = _rpc->request(
        rpc::timeZoneGetMethod, offset,
        [this](rpc::Error error, std::span<const uint8_t> bytes) {
            _request = 0;
            if (!_running) return;
            const bool valid = std::all_of(bytes.begin(), bytes.end(), [](uint8_t c) {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/' ||
                       c == '_' || c == '-' || c == '+';
            });
            if (error != rpc::Error::None || !valid || _pendingZone.size() + bytes.size() > 64 ||
                static_cast<uint32_t>(millis()) - _lastAttempt >= 5000U) {
                _syncState = SyncState::Failed;
                return;
            }
            if (!bytes.empty()) _pendingZone.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            if (bytes.size() == 12)
                _fetchTimeZone();
            else
                _applySample();
        },
        static_cast<uint32_t>(millis()));
    if (!_request) _syncState = SyncState::Failed;
}

void TimeService::_applySample() {
    if (_resyncRequested) {
        _syncState = SyncState::Waiting;
        return;
    }
    const auto sample = decodeClockSample(_pendingSample);
    if (_pendingZone.empty() || !sample || !hal::setClockTime(*sample)) {
        _syncState = SyncState::Failed;
        return;
    }
    uint32_t raw = 0;
    for (int i = 0; i < 4; ++i) raw |= uint32_t(_pendingSample[i + 8]) << (i * 8);
    _utcOffset = static_cast<int32_t>(raw <= 0x7FFFFFFFU ? raw : int64_t(raw) - 0x100000000LL);
    _timeZone = _pendingZone;
    _lastSuccess = static_cast<uint32_t>(millis());
    _synchronized = true;
    _syncState = SyncState::Synchronized;
    _sample(_lastSuccess, true);
}

void TimeService::_sample(uint32_t now, bool initial) {
    _time = hal::clockTime();
    _sampledAt = now;
    // Preserve the existing display policy: advance the minute at second 01.
    if (!initial && _time.second == 0) return;
    if (initial || _time.year != _displayTime.year || _time.month != _displayTime.month ||
        _time.day != _displayTime.day || _time.weekday != _displayTime.weekday || _time.hour != _displayTime.hour ||
        _time.minute != _displayTime.minute) {
        _displayTime = _time;
        ++_minuteRevision;
    }
}

bool TimeService::isRunning() const {
    return _running;
}

const Rtc::DateTime& TimeService::time() const {
    return _time;
}

const Rtc::DateTime& TimeService::displayTime() const {
    return _displayTime;
}

uint64_t TimeService::minuteRevision() const {
    return _minuteRevision;
}

}  // namespace platform::time
