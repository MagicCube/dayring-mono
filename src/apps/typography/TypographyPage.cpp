#include "TypographyPage.h"

#include <FreeInkUILayout.h>

#include <array>

#include "../../platform/fonts/Fonts.h"

namespace apps::typography {
namespace {

using platform::fonts::Font;
using platform::fonts::fontId;
using namespace freeink::ui;

struct ArticleBlock {
    const char* text;
    Font font;
};

constexpr std::array<ArticleBlock, 6> readingArticle{{
    {"The quiet page", Font::Roboto2XL},
    {"A little room to think", Font::RobotoXL},
    {"Finding a slower rhythm", Font::RobotoL},
    {"Morning arrives without a sound. Light crosses the table, and a fresh page waits for the first idea.",
     Font::RobotoM},
    {"We read a little, pause, then begin again. Space between paragraphs gives each thought a place to settle.",
     Font::RobotoM},
    {"Field notes / No. 01\nGood type makes room for words: clear shapes, a steady rhythm, and time to read.",
     Font::RobotoS},
}};

constexpr std::array<ArticleBlock, 6> displayArticle{{
    {"TYPE", Font::NDot120},
    {"IN FOCUS", Font::NDot4XL},
    {"A DAILY RITUAL", Font::NDot2XL},
    {"At 00:13, the room is still. A few points of light mark the hours; a familiar shape becomes a quiet signal.",
     Font::RobotoM},
    {"Large letters set the mood. Smaller words carry the story, leaving enough space for the eye to rest.",
     Font::RobotoM},
    {"Field notes / No. 02\nA study in dots, numbers, and the spaces between them.", Font::RobotoS},
}};

}  // namespace

void TypographyPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props,
                            RenderResult& result) const {
    render(canvas, bounds, props);
    const auto nextX = static_cast<int16_t>(bounds.width * 2 / 3);
    result = {.bounds = bounds,
              .previous = {bounds.x, bounds.y, static_cast<int16_t>(bounds.width / 3), bounds.height},
              .next = {static_cast<int16_t>(bounds.x + nextX), bounds.y, static_cast<int16_t>(bounds.width - nextX),
                       bounds.height}};
}

void TypographyPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds, const Props& props) const {
    canvas.fill(bounds, Paint::solid(Color::White));
    const Rect content = bounds.inset(Insets{.top = 24, .right = 24, .bottom = 24, .left = 24});
    layoutLinear(
        content, Axis::Column, 16, 2,
        [](uint8_t index) { return index == 0 ? LayoutLength::flexible() : LayoutLength::fixed(28); },
        [&](uint8_t index, Rect slot) {
            if (index == 0) {
                _renderArticle(canvas, slot, props);
            } else {
                canvas.text(slot, props.isDisplayArticle ? "02 / NDot 57" : "01 / Roboto",
                            {.font = fontId(Font::RobotoS)});
            }
        });
}

void TypographyPage::_renderArticle(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds,
                                    const Props& props) const {
    const auto& blocks = props.isDisplayArticle ? displayArticle : readingArticle;
    layoutLinear(
        bounds, Axis::Column, 16, static_cast<uint8_t>(blocks.size()),
        [&](uint8_t index) {
            const auto& block = blocks[index];
            const TextStyle style{.font = fontId(block.font), .maxLines = 16};
            return LayoutLength::fixed(measureWrappedText(canvas, block.text, style, bounds.width).height);
        },
        [&](uint8_t index, Rect slot) {
            const auto& block = blocks[index];
            canvas.text(slot, block.text, {.font = fontId(block.font), .maxLines = 16});
        });
}

}  // namespace apps::typography
