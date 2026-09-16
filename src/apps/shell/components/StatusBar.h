#pragma once

#include "../../../platform/ui/Component.h"
#include "../../../platform/ui/MinuteClock.h"

namespace apps::shell::components {
class StatusBar final : public platform::ui::Component {
   public:
    static constexpr int16_t kHeight = 36;
    void reset();
    [[nodiscard]] bool update();
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;

   private:
    platform::ui::MinuteClock _clock;
    uint32_t _chargingSampledAt = 0;
    uint32_t _batterySampledAt = 0;
    uint8_t _percent = 0;
    bool _charging = false;
    bool _sampled = false;
    bool _batteryKnown = false;
};
}  // namespace apps::shell::components
