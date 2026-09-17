#include "ShellApplication.h"

namespace apps::shell {

void ShellApplication::onCreate() {
    (void)router().registerPage("/", _home, "Application launcher. Alias: app://home");
    (void)router().registerPage("/lock", _lock, "Fullscreen clock. Preview time: --time HH:MM");
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
    return navigation().currentPageController() != &_lock;
}

void ShellApplication::update() {
    if (auto* page = navigation().currentPageController(); page && page->update()) requestRender();
}

void ShellApplication::render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) {
    if (auto* page = navigation().currentPageController()) {
        page->render(canvas, bounds);
    }
}

bool ShellApplication::onInput(const platform::runtime::InputEvent& event) {
    auto* page = navigation().currentPageController();
    return page && page->onInput(event);
}

}  // namespace apps::shell
