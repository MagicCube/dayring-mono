#pragma once

#include <Rtc.h>

#include <string>

#include "../../rpc/services/RPCService.h"
#include "../../runtime/services/Service.h"

namespace platform::time {

class TimeService final : public runtime::Service {
   public:
    explicit TimeService(rpc::RPCService* rpc = nullptr) : _rpc(rpc) {
    }

    ~TimeService() override {
        stop();
    }
    enum class SyncState { Waiting, Pending, Synchronized, Failed };

    [[nodiscard]] const std::string& timeZone() const {
        return _timeZone;
    }

    [[nodiscard]] int32_t utcOffsetSeconds() const {
        return _utcOffset;
    }

    [[nodiscard]] bool hasTimeZone() const {
        return !_timeZone.empty();
    }

    [[nodiscard]] SyncState syncState() const {
        return _syncState;
    }

    [[nodiscard]] bool start() override;
    void stop() override;
    void update(uint32_t now) override;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] const Rtc::DateTime& time() const;
    [[nodiscard]] const Rtc::DateTime& displayTime() const;
    [[nodiscard]] uint64_t minuteRevision() const;

   private:
    void _sample(uint32_t now, bool initial);
    void _sync(uint32_t now);
    void _fetchTimeZone();
    void _applySample();
    std::string _timeZone;
    std::string _pendingZone;
    int32_t _utcOffset = 0;
    std::array<uint8_t, 12> _pendingSample{};
    rpc::RPCService* _rpc;
    uint32_t _session = 0;
    uint32_t _lastAttempt = 0;
    uint32_t _lastSuccess = 0;
    uint16_t _request = 0;
    bool _resyncRequested = false;
    bool _attempted = false;
    bool _synchronized = false;
    SyncState _syncState = SyncState::Waiting;
    Rtc::DateTime _time{};
    Rtc::DateTime _displayTime{};
    uint64_t _minuteRevision = 0;
    uint32_t _sampledAt = 0;
    bool _running = false;
};

}  // namespace platform::time
