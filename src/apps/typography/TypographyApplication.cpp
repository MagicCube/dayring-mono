#include "TypographyApplication.h"

namespace apps::typography {
void TypographyApplication::onCreate() {
    (void)router().registerPage("/", _page,
                                "article=reading|display (default: reading). Unknown keys are ignored. "
                                "Example: app://typography/?article=display");
}

void TypographyApplication::onEnter(const platform::runtime::Intent& intent) {
    if (intent.reason != platform::runtime::Intent::Reason::Open) return;
    if (!navigation().replace(intent.url.location.c_str())) (void)navigation().replace("/");
}

void TypographyApplication::onLeave() {
}

void TypographyApplication::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    if (auto* page = navigation().currentPage()) page->render(canvas, bounds);
}

bool TypographyApplication::onInput(const platform::runtime::InputEvent& event) {
    auto* page = navigation().currentPage();
    if (!page || !page->onInput(event)) return false;
    requestRender();
    return true;
}
}  // namespace apps::typography
