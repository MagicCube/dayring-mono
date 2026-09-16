#pragma once

#include "../../../platform/ui/Page.h"

namespace apps::shell::pages {
class HomeScreen final : public platform::ui::Page {
   public:
    void onEnter(std::string_view location) override;
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;
    bool onInput(const platform::runtime::InputEvent& event) override;

   private:
    freeink::ui::InteractionBuffer<3> _interactions;
};
}  // namespace apps::shell::pages
