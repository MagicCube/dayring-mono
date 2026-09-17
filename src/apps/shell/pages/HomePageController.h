#pragma once
#include "../../../platform/ui/PageController.h"
#include "HomePage.h"

namespace apps::shell::pages {

class HomePageController final : public platform::ui::PageController {
   public:
    void onEnter(std::string_view) override;
    void onLeave() override;
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override;
    bool onInput(const platform::runtime::InputEvent&) override;

   private:
    HomePage _view;
    HomePage::RenderResult _rendered;
};

}  // namespace apps::shell::pages
