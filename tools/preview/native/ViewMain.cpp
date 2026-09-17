#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <iostream>
#include <limits>
#include <string_view>

#include "FrameBuffer.h"
#include "apps/common/LandingPage.h"
#include "apps/shell/components/StatusBar.h"
#include "apps/shell/pages/HomePage.h"
#include "apps/shell/pages/LockPage.h"
#include "apps/typography/TypographyPage.h"
#include "platform/fonts/Fonts.h"

namespace {

using platform::ui::Canvas;
using platform::ui::Rect;

struct Example {
    const char* view;
    const char* name;
    Rect bounds;
    void (*render)(Canvas&, const Rect&);
};

constexpr Rect pageBounds{0, 0, 480, 800};
constexpr Rect statusBounds{0, 0, 480, 36};

void renderBatteryExamples(Canvas& canvas, const Rect&) {
    using namespace freeink::ui;
    for (int theme = 0; theme < 2; ++theme) {
        const auto mode = theme == 0 ? platform::ui::Theme::Light : platform::ui::Theme::Dark;
        canvas.fill({static_cast<int16_t>(theme * 240), 0, 240, 800},
                    Paint::solid(theme == 0 ? Color::White : Color::Black));
        const std::array<uint8_t, 6> percentages{0, 1, 10, 50, 85, 100};
        for (size_t i = 0; i < percentages.size(); ++i) {
            for (int charging = 0; charging < 2; ++charging) {
                apps::shell::views::BatteryIndicatorView{}.render(
                    canvas,
                    {static_cast<int16_t>(theme * 240 + 30 + charging * 100), static_cast<int16_t>(40 + i * 100), 60,
                     36},
                    {.percent = percentages[i], .charging = charging != 0, .theme = mode});
            }
        }
    }
}

const std::array examples{
    Example{"BatteryIndicatorView", "themes", pageBounds, renderBatteryExamples},
    Example{"StatusBar", "dark-charging", statusBounds,
            [](Canvas& c, const Rect& b) {
                apps::shell::components::StatusBar{}.render(
                    c, b, {.percent = 85, .charging = true, .theme = platform::ui::Theme::Dark});
            }},
    Example{"HomePage", "default", pageBounds,
            [](Canvas& c, const Rect& b) { (void)apps::shell::pages::HomePage{}.render(c, b, {}); }},
    Example{"LockPage", "default", pageBounds,
            [](Canvas& c, const Rect& b) { apps::shell::pages::LockPage{}.render(c, b, {.hour = 12, .minute = 34}); }},
    Example{"LockPage", "midnight", pageBounds,
            [](Canvas& c, const Rect& b) { apps::shell::pages::LockPage{}.render(c, b, {.hour = 0, .minute = 0}); }},
    Example{
        "TypographyPage", "reading", pageBounds,
        [](Canvas& c, const Rect& b) { apps::typography::TypographyPage{}.render(c, b, {.isDisplayArticle = false}); }},
    Example{
        "TypographyPage", "display", pageBounds,
        [](Canvas& c, const Rect& b) { apps::typography::TypographyPage{}.render(c, b, {.isDisplayArticle = true}); }},
    Example{"LandingPage", "zero", pageBounds,
            [](Canvas& c, const Rect& b) {
                (void)apps::common::LandingPage{}.render(c, b, {.title = "Calendar", .count = 0});
            }},
    Example{"LandingPage", "maximum", pageBounds,
            [](Canvas& c, const Rect& b) {
                (void)apps::common::LandingPage{}.render(c, b,
                                                         {.title = "Test", .count = std::numeric_limits<int>::max()});
            }},
    Example{"LandingPage", "minimum", pageBounds,
            [](Canvas& c, const Rect& b) {
                (void)apps::common::LandingPage{}.render(c, b,
                                                         {.title = "Test", .count = std::numeric_limits<int>::min()});
            }},
    Example{"StatusBar", "default", statusBounds,
            [](Canvas& c, const Rect& b) { apps::shell::components::StatusBar{}.render(c, b, {}); }},
    Example{"StatusBar", "charging", statusBounds,
            [](Canvas& c, const Rect& b) {
                apps::shell::components::StatusBar{}.render(c, b,
                                                            {.hour = 9, .minute = 15, .percent = 10, .charging = true});
            }},
    Example{"StatusBar", "empty", statusBounds,
            [](Canvas& c, const Rect& b) {
                apps::shell::components::StatusBar{}.render(c, b, {.hour = 0, .minute = 0, .percent = 0});
            }},
};

int fail(const char* code, const char* message) {
    std::cout << "{\"error\":{\"code\":\"" << code << "\",\"message\":\"" << message << "\"}}\n";
    return 3;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string_view command = argc > 1 ? argv[1] : "";
    if (command == "views" && argc == 2) {
        std::cout << "{\"protocol\":1,\"examples\":[";
        bool first = true;
        for (const auto& example : examples) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << "{\"view\":\"" << example.view << "\",\"name\":\"" << example.name << "\",\"bounds\":[0,0,"
                      << example.bounds.width << ',' << example.bounds.height << "]}";
        }
        std::cout << "]}\n";
        return 0;
    }
    if (command != "capture-view" || argc != 4) return fail("invalid_arguments", "Use the preview CLI.");
    bool knownView = false;
    for (const auto& example : examples) {
        if (example.view != std::string_view(argv[2])) continue;
        knownView = true;
        if (example.name != std::string_view(argv[3])) continue;
        preview::resetFrame();
        freeink::ui::DisplayTarget canvas(preview::framePixels().data(), 800, 480, 100,
                                          freeink::ui::Orientation::Portrait);
        canvas.setGrayPreview(preview::grayPixels().data());
        platform::fonts::registerFonts(canvas);
        example.render(canvas, example.bounds);
        return preview::writeFrame("");
    }
    return fail(knownView ? "unknown_example" : "unknown_view", "Run views or view-help to discover examples.");
}
