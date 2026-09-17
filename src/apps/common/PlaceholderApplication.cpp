#include "PlaceholderApplication.h"

namespace apps::common {

PlaceholderApplication::PlaceholderApplication(std::string title) : _page(std::move(title)) {
}

void PlaceholderApplication::onCreate() {
    (void)router().registerPage("/", _page, "Placeholder with counter and Home action. No query parameters.");
}

void PlaceholderApplication::onEnter(const platform::runtime::Intent& intent) {
    if (intent.reason != platform::runtime::Intent::Reason::Open) return;
    if (!navigation().replace(intent.url.location.c_str())) (void)navigation().replace("/");
}

void PlaceholderApplication::onLeave() {
}

void PlaceholderApplication::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    if (auto* page = navigation().currentPageController()) {
        page->render(canvas, bounds);
    }
}

bool PlaceholderApplication::onInput(const platform::runtime::InputEvent& event) {
    auto* page = navigation().currentPageController();
    if (!page || !page->onInput(event)) return false;
    requestRender();
    return true;
}

}  // namespace apps::common
