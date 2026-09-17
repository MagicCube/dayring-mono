#include "HomePage.h"

#include <FreeInkUILayout.h>
#include <components/controls/button.h>

#include <algorithm>

namespace apps::shell::pages {

void HomePage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props) const {
    RenderResult result;
    render(canvas, bounds, props, result);
}

void HomePage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props&,
                      RenderResult& result) const {
    canvas.fill(bounds, freeink::ui::Paint::solid(freeink::ui::Color::White));
    const freeink::ui::DeviceContext device{.width = bounds.right(), .height = bounds.bottom()};
    const freeink::ui::InputSnapshot input{};
    freeink::ui::InteractionBuffer<3> interactions;
    freeink::ui::Frame<3> frame(canvas, device, input, interactions);
    auto styles = freeink::ui::defaultButtonStyles();
    styles.normal.border = freeink::ui::Paint::solid(freeink::ui::Color::Black);
    styles.normal.borderWidth = 2;

    constexpr int16_t padding = 24;
    constexpr int16_t gap = 24;
    const auto width = static_cast<int16_t>(std::clamp<int>(bounds.width - padding * 2, 0, 320));
    constexpr int16_t height = 64;
    const auto x = static_cast<int16_t>(bounds.x + (bounds.width - width) / 2);
    const auto totalHeight = static_cast<int16_t>(height * 3 + gap * 2);
    const auto y = static_cast<int16_t>(bounds.y + (bounds.height - totalHeight) / 2);
    const freeink::ui::Rect menu{x, y, width, totalHeight};
    constexpr const char* labels[]{"Calendar", "Test", "Typography"};
    freeink::ui::layoutLinear(
        menu, freeink::ui::Axis::Column, gap, 3, [](uint8_t) { return freeink::ui::LayoutLength::fixed(height); },
        [&](uint8_t index, freeink::ui::Rect slot) {
            freeink::ui::button(frame, slot,
                                {.label = labels[index],
                                 .action = static_cast<freeink::ui::ActionId>(index + 1),
                                 .inputMask = freeink::ui::InputTouch,
                                 .styles = styles,
                                 .radius = 12});
        });
    // The output buffer's interaction session must not become a rendering input.
    result.interactions.clear();
    for (std::size_t index = 0; index < interactions.count(); ++index) {
        result.interactions.addInteraction(interactions.data()[index]);
    }
}

}  // namespace apps::shell::pages
