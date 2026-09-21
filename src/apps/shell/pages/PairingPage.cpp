#include "PairingPage.h"

#include "../../../platform/fonts/Fonts.h"

namespace apps::shell::pages {

void PairingPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props) const {
    using namespace freeink::ui;
    using platform::fonts::Font;
    using platform::fonts::fontId;
    canvas.fill(bounds, Paint::solid(Color::Black));
    const auto label = [&](int16_t y, const char* text, Font font) {
        const auto id = fontId(font);
        canvas.text({bounds.x, static_cast<int16_t>(bounds.y + y), bounds.width, canvas.lineHeight(id)}, text,
                    {.font = id, .align = TextAlign::Center, .color = Color::White});
    };
    label(76, "HELLO", Font::NDot4XL);
    label(170, "Let's get started", Font::RobotoM);
    platform::ui::QRCodeView{}.render(
        canvas, {static_cast<int16_t>(bounds.x + 108), static_cast<int16_t>(bounds.y + 274), 264, 264},
        {.code = props.code, .cornerRadius = 16});
    label(561, "dayring.ai", Font::RobotoL);
    label(619, "Scan to get the Dayring app", Font::RobotoS);
    label(719, props.pairing ? "Pairing..." : "Waiting for pairing...", Font::RobotoS);
}

}  // namespace apps::shell::pages
