#pragma once

#include "../../../platform/ui/Page.h"

namespace apps::shell::pages {

struct FirmwareUpdatePageProps {};

class FirmwareUpdatePage final : public platform::ui::Page<FirmwareUpdatePageProps> {
   public:
    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override;
};

}  // namespace apps::shell::pages
