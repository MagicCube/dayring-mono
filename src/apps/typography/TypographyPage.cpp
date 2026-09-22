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
    const char* translation = nullptr;
    Font translationFont = Font::RobotoL;
    int16_t translationOffset = 0;
};

constexpr std::array<ArticleBlock, 6> readingArticle{{
    {"字里行间 / Type", Font::Roboto2XL},
    {"慢一点，Read on.", Font::RobotoXL},
    {"李昕 / Hello, world!", Font::RobotoL},
    {"清晨，翻开新的一页。\nA quiet moment to read.", Font::RobotoM},
    {"“你好，世界！”《日常》\n(Hello, world!) 09:30 — 12:00", Font::RobotoM},
    {"小字也清晰 / Small details\n《阅读》、“你好”…… / Read on.\n标点对照：，。！？；：（） / ,.!?;:()", Font::RobotoS},
}};

constexpr std::array<ArticleBlock, 6> displayArticle{{
    {"TYPE", Font::NDot120, "字形", Font::Roboto2XL, 47},
    {"FOCUS", Font::NDot4XL, "专注", Font::RobotoXL, 22},
    {"DAILY", Font::NDot2XL, "日常", Font::RobotoM, 9},
    {"夜深了，Time slows down.\n00:13，留一点时间给自己。", Font::RobotoM},
    {"大字定下节奏，Small words tell stories.\n在点与线之间，Find your rhythm.", Font::RobotoM},
    {"字形笔记 / Field notes\n点阵、数字与留白。Dots, numbers, space.", Font::RobotoS},
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
                canvas.text(slot, props.isDisplayArticle ? "02 / NDot 57 + 思源简体" : "01 / Roboto + 思源简体",
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
            if (block.translation) {
                const auto width = canvas.measureText(fontId(block.font), block.text, {}).width;
                const auto height = canvas.lineHeight(fontId(block.translationFont));
                const Rect translation{static_cast<int16_t>(slot.x + width + 16),
                                       static_cast<int16_t>(slot.y + block.translationOffset),
                                       static_cast<int16_t>(slot.width - width - 16), height};
                canvas.text(translation, block.translation, {.font = fontId(block.translationFont)});
            }
        });
}

}  // namespace apps::typography
