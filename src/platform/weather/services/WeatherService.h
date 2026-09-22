#pragma once

#include "../../rpc/services/RPCService.h"
#include "../WeatherStorage.h"

namespace platform::weather {

class WeatherService final : public runtime::Service {
   public:
    enum class SyncState { Waiting, Pending, Synchronized, Failed };
    using DateProvider = std::function<std::optional<uint32_t>()>;
    enum class PersistenceState { Unavailable, Pending, Saved, Failed };
    explicit WeatherService(rpc::RPCService& rpc, DateProvider date = {},
                            std::unique_ptr<WeatherStorage> storage = makeWeatherStorage());
    ~WeatherService() override;
    bool start() override;
    void stop() override;
    void update(uint32_t now) override;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] SyncState syncState() const;
    [[nodiscard]] uint64_t reportRevision() const;
    [[nodiscard]] PersistenceState persistenceState() const;
    // Same-day report restored from FATFS or received from the peer.
    [[nodiscard]] const std::optional<WeatherReport>& report() const;

   private:
    void _updateDate();
    void _consumeReply(uint32_t now);
    void _persist(uint32_t now);
    void _setReport(std::optional<WeatherReport> report);
    rpc::RPCService& _rpc;
    DateProvider _dateProvider;
    std::unique_ptr<WeatherStorage> _storage;
    std::optional<CachedWeather> _cache;
    std::optional<uint32_t> _date;
    std::optional<uint32_t> _requestDate;
    PersistenceState _persistenceState = PersistenceState::Unavailable;
    uint32_t _savedAt = 0;
    bool _savePending = false;
    bool _saveAttempted = false;
    std::optional<WeatherReport> _report;
    SyncState _syncState = SyncState::Waiting;
    uint64_t _reportRevision = 0;
    uint32_t _session = 0;
    std::optional<WeatherReport> _reply;
    bool _hasReply = false;
    uint32_t _completedAt = 0;
    uint32_t _delay = 0;
    uint16_t _request = 0;
    bool _running = false;
};

}  // namespace platform::weather
