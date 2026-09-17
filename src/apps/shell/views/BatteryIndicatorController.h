#pragma once
#include "../../../platform/ui/ViewController.h"
#include "BatteryIndicatorView.h"

namespace apps::shell::views {

class BatteryIndicatorController final : public platform::ui::ViewController {
   public:
    void reset();
    [[nodiscard]] bool update() override;
    [[nodiscard]] BatteryIndicatorProps props(platform::ui::Theme theme = platform::ui::Theme::Light) const;
    void setTheme(platform::ui::Theme theme);
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override;

   private:
    BatteryIndicatorView _view;
    bool _themeChanged = false;
    platform::ui::Theme _theme = platform::ui::Theme::Light;
    uint32_t _chargingSampledAt = 0;
    uint32_t _batterySampledAt = 0;
    uint8_t _percent = 0;
    bool _charging = false;
    bool _sampled = false;
    bool _batteryKnown = false;
};

}  // namespace apps::shell::views
