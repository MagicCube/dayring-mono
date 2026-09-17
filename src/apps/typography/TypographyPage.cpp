#include "TypographyPage.h"

#include <FreeInkUILayout.h>

#include <array>
#include <cstdlib>

#include "../../platform/fonts/Fonts.h"
#include "../../platform/runtime/AppURL.h"
#include "../../platform/runtime/Shell.h"

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

bool TypographyPage::acceptsLocation(std::string_view location) const {
    const auto query = platform::runtime::LocationQuery::parse(location);
    if (!query) return false;
    const auto article = query->value("article");
    return !article || *article == "reading" || *article == "display";
}

void TypographyPage::onEnter(std::string_view location) {
    const auto query = platform::runtime::LocationQuery::parse(location);
    _isDisplayArticle = query && query->value("article") == "display";
    _hasRendered = false;
}

void TypographyPage::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    _renderedBounds = bounds;
    _hasRendered = true;
    canvas.fill(bounds, Paint::solid(Color::White));
    const Rect content = bounds.inset(Insets{.top = 24, .right = 24, .bottom = 24, .left = 24});
    layoutLinear(
        content, Axis::Column, 16, 2,
        [](uint8_t index) { return index == 0 ? LayoutLength::flexible() : LayoutLength::fixed(28); },
        [&](uint8_t index, Rect slot) {
            if (index == 0) {
                _renderArticle(canvas, slot);
            } else {
                canvas.text(slot, _isDisplayArticle ? "02 / NDot 57" : "01 / Roboto", {.font = fontId(Font::RobotoS)});
            }
        });
}

void TypographyPage::_renderArticle(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    const auto& blocks = _isDisplayArticle ? displayArticle : readingArticle;
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

bool TypographyPage::onInput(const platform::runtime::InputEvent& event) {
    using Type = platform::runtime::InputEvent::Type;
    if (event.type == Type::Back) return platform::runtime::Shell::instance().goHome();
    if (!_hasRendered) return false;
    bool next;
    if (event.type == Type::TouchRelease && _renderedBounds.contains(event.x, event.y)) {
        const auto relativeX = event.x - _renderedBounds.x;
        if (relativeX < _renderedBounds.width / 3) {
            next = false;
        } else if (relativeX >= _renderedBounds.width * 2 / 3) {
            next = true;
        } else {
            return false;
        }
    } else if (event.type == Type::Swipe && _renderedBounds.contains(event.startX, event.startY)) {
        const int dx = event.x - event.startX;
        const int dy = event.y - event.startY;
        if (std::abs(dx) < 40 || std::abs(dx) <= std::abs(dy)) return false;
        next = dx < 0;
    } else {
        return false;
    }
    if (next == _isDisplayArticle) return false;
    _isDisplayArticle = next;
    _hasRendered = false;
    return true;
}
}  // namespace apps::typography
