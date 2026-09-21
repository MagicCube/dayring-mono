#pragma once

#include <cstdint>

#include "../../runtime/services/Service.h"
#include "FrontlightService.h"

namespace platform::power {

class PowerService final : public runtime::Service {
   public:
    explicit PowerService(frontlight::FrontlightService& frontlight);

    enum class LockReason { Manual, Idle };
    static constexpr uint32_t lockInteractionDurationMs = 8000;

    [[nodiscard]] bool start() override;
    void stop() override;
    void update(uint32_t now) override;
    [[nodiscard]] bool isRunning() const;
    void notifyActivity();
    void notifyPowerConnectionChanged();
    void notifyLockInteraction();
    void toggleLockedLight();
    void setLocked(bool locked, LockReason reason = LockReason::Manual);
    [[nodiscard]] bool isIdleLockDue() const;

   private:
    void _updateFrontlight();
    void _activateFrontlight();

    static constexpr uint8_t _frontlightActiveBrightness = 20;
    static constexpr uint8_t _frontlightDimmedBrightness = 10;
    static constexpr uint32_t _frontlightDimTimeoutMs = 52000;
    static constexpr uint32_t _frontlightOffTimeoutMs = 60000;
    static constexpr uint32_t _frontlightLockTimeoutMs = 10000;
    bool _running = false;
    uint32_t _frontlightLastActivityMs = 0;
    uint32_t _lockedLightDurationMs = _frontlightLockTimeoutMs;
    frontlight::FrontlightService& _frontlight;
    bool _locked = false;
    bool _hasLocked = false;
    bool _lockInteractionLightActive = false;
};

}  // namespace platform::power
