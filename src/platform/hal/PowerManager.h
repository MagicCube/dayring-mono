#pragma once

#include <FrontlightManager.h>

#include <cstdint>

namespace platform::hal {
class PowerManager {
   public:
    enum class LockReason { Manual, Idle };

    void begin();
    void update(bool inputActive = false);
    void notifyActivity();
    void setLocked(bool locked, LockReason reason = LockReason::Manual);
    [[nodiscard]] bool isIdleLockDue() const;

   private:
    void _beginFrontlight();
    void _updateFrontlight();
    void _activateFrontlight();
    void _setFrontlightBrightness(uint8_t percent);

    static constexpr uint8_t _frontlightActiveBrightness = 20;
    static constexpr uint8_t _frontlightDimmedBrightness = 10;
    static constexpr uint32_t _frontlightDimTimeoutMs = 52000;
    static constexpr uint32_t _frontlightOffTimeoutMs = 60000;
    static constexpr uint32_t _frontlightLockTimeoutMs = 10000;
    FrontlightManager _frontlightDriver;
    uint32_t _frontlightLastActivityMs = 0;
    uint8_t _frontlightBrightness = 0;
    bool _locked = false;
    bool _hasLocked = false;
};
}  // namespace platform::hal
