#include "ShellApplication.h"

namespace apps::shell {
void ShellApplication::onCreate() {
    (void)router().registerPage("/", _home);
    (void)router().registerPage("/lock", _lock);
}

void ShellApplication::onEnter(const platform::runtime::Intent& intent) {
    if (intent.reason != platform::runtime::Intent::Reason::Open) return;
    if (!navigation().replace(intent.url.location.c_str())) {
        (void)navigation().replace("/");
    }
}

void ShellApplication::onLeave() {
}

bool ShellApplication::allowsIdleLock() const {
    return navigation().currentPage() != &_lock;
}

void ShellApplication::update() {
    if (auto* page = navigation().currentPage(); page && page->update()) requestRender();
}

void ShellApplication::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    if (auto* page = navigation().currentPage()) {
        page->render(canvas, bounds);
    }
}

bool ShellApplication::onInput(const platform::runtime::InputEvent& event) {
    auto* page = navigation().currentPage();
    return page && page->onInput(event);
}
}  // namespace apps::shell
