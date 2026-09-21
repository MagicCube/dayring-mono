#pragma once

#include "../../../platform/ui/Page.h"
#include "../../../platform/ui/QRCodeView.h"

namespace apps::shell::pages {

struct PairingPageProps {
    const platform::ui::QRCode* code = nullptr;
    bool pairing = false;
};

class PairingPage final : public platform::ui::Page<PairingPageProps> {
   public:
    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override;
};

}  // namespace apps::shell::pages
