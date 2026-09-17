#pragma once

#include <Rtc.h>

#include "../../runtime/services/Service.h"

namespace platform::time {

class TimeService final : public runtime::Service {
   public:
    [[nodiscard]] bool start() override;
    void stop() override;
    void update(uint32_t now) override;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] const Rtc::DateTime& time() const;
    [[nodiscard]] const Rtc::DateTime& displayTime() const;
    [[nodiscard]] uint64_t minuteRevision() const;

   private:
    void _sample(uint32_t now, bool initial);
    Rtc::DateTime _time{};
    Rtc::DateTime _displayTime{};
    uint64_t _minuteRevision = 0;
    uint32_t _sampledAt = 0;
    bool _running = false;
};

}  // namespace platform::time
