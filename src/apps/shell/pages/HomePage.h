#pragma once
#include "../../../platform/ui/Page.h"

namespace apps::shell::pages {

struct HomePageProps {};

class HomePage final : public platform::ui::Page<HomePageProps> {
   public:
    struct RenderResult {
        freeink::ui::InteractionBuffer<3> interactions;
    };

    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override;
    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&, RenderResult&) const;
};

}  // namespace apps::shell::pages
