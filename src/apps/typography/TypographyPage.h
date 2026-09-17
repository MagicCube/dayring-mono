#pragma once
#include "../../platform/ui/Page.h"

namespace apps::typography {

struct TypographyPageProps {
    bool isDisplayArticle = false;
};

class TypographyPage final : public platform::ui::Page<TypographyPageProps> {
   public:
    struct RenderResult {
        platform::ui::Rect bounds;
        platform::ui::Rect previous;
        platform::ui::Rect next;
    };

    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override;
    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&, RenderResult&) const;

   private:
    void _renderArticle(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const;
};

}  // namespace apps::typography
