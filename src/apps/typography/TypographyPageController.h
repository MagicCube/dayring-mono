#pragma once

#include "../../platform/ui/PageController.h"
#include "TypographyPage.h"

namespace apps::typography {

class TypographyPageController final : public platform::ui::PageController {
   public:
    [[nodiscard]] bool acceptsLocation(std::string_view location) const override;
    void onEnter(std::string_view location) override;
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;

    void onLeave() override {
        _hasRendered = false;
    }

    bool onInput(const platform::runtime::InputEvent& event) override;

   private:
    TypographyPage _view;
    bool _isDisplayArticle = false;
    TypographyPage::RenderResult _rendered{};
    bool _hasRendered = false;
};

}  // namespace apps::typography
