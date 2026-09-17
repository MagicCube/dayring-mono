#pragma once
#include "../../../platform/ui/Theme.h"
#include "../../../platform/ui/View.h"

namespace apps::shell::views {

struct BatteryIndicatorProps {
    uint8_t percent = 0;
    bool charging = false;
    platform::ui::Theme theme = platform::ui::Theme::Light;
};

class BatteryIndicatorView final : public platform::ui::View<BatteryIndicatorProps> {
   public:
    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override;
};

}  // namespace apps::shell::views
