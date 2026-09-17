#include "FirmwareUpdatePage.h"

#include "../../../platform/fonts/Fonts.h"

namespace apps::shell::pages {

void FirmwareUpdatePage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props&) const {
    using platform::fonts::Font;
    using platform::fonts::fontId;
    using namespace freeink::ui;
    canvas.fill(bounds, Paint::solid(Color::Black));
    constexpr auto titleFont = fontId(Font::NDot2XL);
    constexpr auto labelFont = fontId(Font::RobotoM);
    const auto titleHeight = canvas.lineHeight(titleFont);
    const auto labelHeight = canvas.lineHeight(labelFont);
    const auto height = titleHeight * 2 + 12;
    const auto top = static_cast<int16_t>(bounds.y + (bounds.height - height) / 2);
    canvas.text({bounds.x, top, bounds.width, titleHeight}, "UPDATING",
                {.font = titleFont, .align = TextAlign::Center, .color = Color::White});
    canvas.text({bounds.x, static_cast<int16_t>(top + titleHeight + 12), bounds.width, titleHeight}, "FIRMWARE",
                {.font = titleFont, .align = TextAlign::Center, .color = Color::White});
    canvas.text(
        {bounds.x, static_cast<int16_t>(bounds.y + bounds.height - 40 - labelHeight), bounds.width, labelHeight},
        "Keep USB Connected", {.font = labelFont, .align = TextAlign::Center, .color = Color::LightGray});
}

}  // namespace apps::shell::pages
