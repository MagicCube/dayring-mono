#pragma once

#include "../../../platform/ui/MinuteClock.h"
#include "../../../platform/ui/Page.h"

namespace apps::shell::pages {
class LockScreen final : public platform::ui::Page {
   public:
    LockScreen() : Page(true) {
    }
    void onEnter(std::string_view location) override;
    [[nodiscard]] bool update() override;
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;

   private:
    platform::ui::MinuteClock _clock;
};
}  // namespace apps::shell::pages
