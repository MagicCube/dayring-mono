#pragma once
#include <cstdint>

#include "../../../platform/ui/ViewController.h"
#include "../views/BatteryIndicatorController.h"
#include "StatusBar.h"

namespace apps::shell::components {

class StatusBarController final : public platform::ui::ViewController {
   public:
    void reset();
    void setTheme(platform::ui::Theme theme);
    [[nodiscard]] bool update() override;
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;

   private:
    StatusBar _view;
    platform::ui::Theme _theme = platform::ui::Theme::Light;
    bool _themeChanged = false;
    uint64_t _minuteRevision = 0;
    views::BatteryIndicatorController _battery;
};

}  // namespace apps::shell::components
