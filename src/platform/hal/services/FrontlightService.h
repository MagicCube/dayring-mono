#pragma once

#include <cstdint>

#include "../../runtime/services/Service.h"

namespace platform::frontlight {

class FrontlightService final : public runtime::Service {
   public:
    [[nodiscard]] bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] uint8_t brightness() const;
    [[nodiscard]] bool isOn() const;
    void setBrightness(uint8_t percent);
    void turnOn();
    void turnOff();

   private:
    uint8_t _lastOnBrightness = 20;
    bool _running = false;
};

}  // namespace platform::frontlight
