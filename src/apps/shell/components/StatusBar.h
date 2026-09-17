#pragma once
#include "../../../platform/ui/View.h"
#include "../views/BatteryIndicatorView.h"

namespace apps::shell::components {

struct StatusBarProps {
    uint8_t hour = 12;
    uint8_t minute = 34;
    uint8_t percent = 75;
    bool charging = false;
    platform::ui::Theme theme = platform::ui::Theme::Light;
};

class StatusBar final : public platform::ui::View<StatusBarProps> {
   public:
    static constexpr int16_t kHeight = 36;

    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override;
};

}  // namespace apps::shell::components
