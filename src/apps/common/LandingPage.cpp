#include "LandingPage.h"

#include <components/controls/button.h>

#include <algorithm>
#include <string>

namespace apps::common {

void LandingPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props) const {
    RenderResult result;
    render(canvas, bounds, props, result);
}

void LandingPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props,
                         RenderResult& result) const {
    canvas.fill(bounds, freeink::ui::Paint::solid(freeink::ui::Color::White));
    const freeink::ui::DeviceContext device{.width = bounds.right(), .height = bounds.bottom()};
    const freeink::ui::InputSnapshot input{};
    freeink::ui::InteractionBuffer<3> interactions;
    freeink::ui::Frame<3> frame(canvas, device, input, interactions);
    const auto width = static_cast<int16_t>(std::clamp<int>(bounds.width - 48, 0, 320));
    const auto x = static_cast<int16_t>(bounds.x + (bounds.width - width) / 2);
    const auto y = static_cast<int16_t>(bounds.y + bounds.height / 2);
    canvas.text({x, static_cast<int16_t>(y - 88), width, 64}, props.title,
                {.align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::Black});
    freeink::ui::button(frame, {x, y, width, 64},
                        {.label = "Back to Home", .action = 1, .inputMask = freeink::ui::InputTouch, .radius = 12});
    const auto value = std::to_string(props.count);
    canvas.text({x, static_cast<int16_t>(y + 88), width, 64}, value.c_str(),
                {.align = freeink::ui::TextAlign::Center, .color = freeink::ui::Color::Black});
    const auto buttonWidth = static_cast<int16_t>(std::max<int>(0, (width - 24) / 2));
    freeink::ui::button(frame, {x, static_cast<int16_t>(y + 176), buttonWidth, 64},
                        {.label = "-", .action = 2, .inputMask = freeink::ui::InputTouch, .radius = 12});
    freeink::ui::button(frame,
                        {static_cast<int16_t>(x + width - buttonWidth), static_cast<int16_t>(y + 176), buttonWidth, 64},
                        {.label = "+", .action = 3, .inputMask = freeink::ui::InputTouch, .radius = 12});
    // The output buffer's interaction session must not become a rendering input.
    result.interactions.clear();
    for (std::size_t index = 0; index < interactions.count(); ++index) {
        result.interactions.addInteraction(interactions.data()[index]);
    }
}

}  // namespace apps::common
