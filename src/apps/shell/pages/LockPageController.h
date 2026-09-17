#pragma once
#include <cstdint>

#include "../../../platform/ui/PageController.h"
#include "LockPage.h"

namespace apps::shell::pages {

class LockPageController final : public platform::ui::PageController {
   public:
    LockPageController() : PageController(true) {
    }

    void onEnter(std::string_view) override;
    bool update() override;
    bool onInput(const platform::runtime::InputEvent&) override;
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override;

   private:
    uint64_t _minuteRevision = 0;
    LockPage _view;
    bool _samplePower(bool refreshPercent);
    uint32_t _powerSampledAt = 0;
    bool _powerSampled = false;
    bool _powerKnown = false;
    bool _charging = false;
    bool _batteryKnown = false;
    uint8_t _percent = 0;
    bool _showUnlockHint = false;
    bool _hintChanged = false;
    uint32_t _hintShownAt = 0;
};

}  // namespace apps::shell::pages
