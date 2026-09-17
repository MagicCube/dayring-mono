#pragma once
#include <string>

#include "../../platform/ui/PageController.h"
#include "LandingPage.h"

namespace apps::common {

class LandingPageController final : public platform::ui::PageController {
   public:
    explicit LandingPageController(std::string title, int initialCount = 0);
    void onEnter(std::string_view) override;
    void onLeave() override;
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override;
    bool onInput(const platform::runtime::InputEvent&) override;

   private:
    std::string _title;
    int _count = 0;
    LandingPage _view;
    LandingPage::RenderResult _rendered;
};

}  // namespace apps::common
