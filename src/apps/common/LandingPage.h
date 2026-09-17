#pragma once
#include "../../platform/ui/Page.h"

namespace apps::common {

struct LandingPageProps {
    const char* title = "";
    int count = 0;
};

class LandingPage final : public platform::ui::Page<LandingPageProps> {
   public:
    struct RenderResult {
        freeink::ui::InteractionBuffer<3> interactions;
    };

    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override;
    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&, RenderResult&) const;
};

}  // namespace apps::common
