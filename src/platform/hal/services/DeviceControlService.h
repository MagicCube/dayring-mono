#pragma once

#include "../../ble/services/BLEService.h"
#include "../../rpc/services/RPCService.h"

namespace platform::hal {

class DeviceControlService final : public runtime::Service {
   public:
    DeviceControlService(rpc::RPCService& rpc, ble::BLEService& ble);
    ~DeviceControlService() override;
    bool start() override;
    void stop() override;
    void update(uint32_t now) override;

   private:
    rpc::Reply _resetPairing(std::span<const uint8_t> payload);
    rpc::Reply _reboot(std::span<const uint8_t> payload);
    void _scheduleReboot();
    rpc::RPCService& _rpc;
    ble::BLEService& _ble;
    uint32_t _resetSession = 0;
    uint32_t _restartAt = 0;
    uint32_t _resetCompletedAt = 0;
    bool _resetCompleted = false;
    bool _running = false;
    bool _restartPending = false;
};

}  // namespace platform::hal
