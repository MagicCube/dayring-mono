#include "WeatherService.h"

namespace platform::weather {

WeatherService::WeatherService(rpc::RPCService& rpc, DateProvider date, std::unique_ptr<WeatherStorage> storage)
    : _rpc(rpc), _dateProvider(std::move(date)), _storage(std::move(storage)) {
}

WeatherService::~WeatherService() {
    stop();
}

bool WeatherService::start() {
    if (_running) return true;
    _hasReply = false;
    _session = 0;
    _delay = 0;
    _syncState = SyncState::Waiting;
    _running = true;
    if (!_cache && _storage) {
        _cache = _storage->load();
        if (_cache) _persistenceState = PersistenceState::Saved;
    }
    _updateDate();
    return true;
}

void WeatherService::stop() {
    _running = false;
    if (_request) _rpc.cancel(_request);
    _request = 0;
}

void WeatherService::update(uint32_t now) {
    if (!_running) return;
    _updateDate();
    const auto session = _rpc.session();
    if (session != _session) {
        if (_request) _rpc.cancel(_request);
        _request = 0;
        _hasReply = false;
        _session = session;
        _delay = 0;
        _syncState = SyncState::Waiting;
    }
    if (_hasReply) _consumeReply(now);
    _persist(now);
    if (!session || _request || (_dateProvider && !_date) || now - _completedAt < _delay) return;
    _requestDate = _date;
    _syncState = SyncState::Pending;
    _request = _rpc.request(
        weatherGetMethod, {},
        [this](rpc::Error error, std::span<const uint8_t> bytes) {
            _request = 0;
            if (!_running) return;
            _reply = error == rpc::Error::None ? decodeWeatherReport(bytes) : std::nullopt;
            _hasReply = true;
        },
        now);
    if (!_request) {
        _syncState = SyncState::Failed;
        _completedAt = now;
        _delay = 30000;
    }
}

void WeatherService::_updateDate() {
    const auto date = _dateProvider ? _dateProvider() : std::nullopt;
    if (date == _date) return;
    _date = date;
    if (_request) _rpc.cancel(_request);
    _request = 0;
    _hasReply = false;
    _reply.reset();
    _delay = 0;
    _syncState = SyncState::Waiting;
    _savePending = false;
    _setReport(_date && _cache && _cache->date == *_date ? std::optional{_cache->report} : std::nullopt);
}

void WeatherService::_setReport(std::optional<WeatherReport> report) {
    if (report == _report) return;
    _report = std::move(report);
    ++_reportRevision;
}

void WeatherService::_consumeReply(uint32_t now) {
    _hasReply = false;
    _completedAt = now;
    _delay = _reply ? 3600000U : 30000U;
    _syncState = _reply ? SyncState::Synchronized : SyncState::Failed;
    if (_reply) {
        _setReport(_reply);
        if (_requestDate) {
            const CachedWeather value{*_requestDate, *_reply};
            if (!_cache || *_cache != value || _persistenceState != PersistenceState::Saved) {
                _cache = value;
                _savePending = true;
                _saveAttempted = false;
                _persistenceState = PersistenceState::Pending;
            }
        }
    }
    _reply.reset();
}

void WeatherService::_persist(uint32_t now) {
    if (!_savePending || !_cache || (_saveAttempted && now - _savedAt < 30000U)) return;
    _savedAt = now;
    _saveAttempted = true;
    if (_storage && _storage->save(*_cache)) {
        _savePending = false;
        _persistenceState = PersistenceState::Saved;
    } else {
        _persistenceState = PersistenceState::Failed;
    }
}

WeatherService::PersistenceState WeatherService::persistenceState() const {
    return _persistenceState;
}

bool WeatherService::isRunning() const {
    return _running;
}

WeatherService::SyncState WeatherService::syncState() const {
    return _syncState;
}

uint64_t WeatherService::reportRevision() const {
    return _reportRevision;
}

const std::optional<WeatherReport>& WeatherService::report() const {
    return _report;
}

}  // namespace platform::weather
