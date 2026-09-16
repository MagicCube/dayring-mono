#pragma once

#include "../../platform/ui/Page.h"

namespace apps::typography {
class TypographyPage final : public platform::ui::Page {
   public:
    void onEnter(std::string_view location) override;
    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override;
    bool onInput(const platform::runtime::InputEvent& event) override;

   private:
    void _renderArticle(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds);
    bool _isDisplayArticle = false;
    platform::ui::Rect _renderedBounds{};
    bool _hasRendered = false;
};
}  // namespace apps::typography
