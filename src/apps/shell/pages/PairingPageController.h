#pragma once

#include "../../../platform/ui/PageController.h"
#include "PairingPage.h"

namespace apps::shell::pages {

class PairingPageController final : public platform::ui::PageController {
   public:
    PairingPageController();
    void onEnter(std::string_view) override;
    bool update() override;
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override;

   private:
    PairingPage _view;
    platform::ui::QRCode _code;
    bool _pairing = false;
};

}  // namespace apps::shell::pages
