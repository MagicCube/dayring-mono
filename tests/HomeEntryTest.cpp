#include <cassert>
#include <iostream>
#include <memory>
#include <string_view>

#include "platform/runtime/Shell.h"
#include "platform/ui/PageController.h"

namespace platform::hal {

void testFrontlightBrightness(uint8_t) {
}

}  // namespace platform::hal

namespace {

using platform::runtime::Application;
using platform::runtime::ApplicationManager;
using platform::runtime::Intent;
Intent received;
int enters = 0;
int leaves = 0;

class HomePageController final : public platform::ui::PageController {
   public:
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }
};

class HomeApplication final : public Application {
   public:
    void onCreate() override {
        assert(router().registerPage("/", _page));
        assert(router().registerPage("/path/to/page", _page));
    }

    void onEnter(const Intent& intent) override {
        ++enters;
        received = intent;
        assert(navigation().replace(intent.url.location.c_str()));
    }

    void onLeave() override {
        ++leaves;
    }

    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }

   private:
    HomePageController _page;
};

std::unique_ptr<Application> createApplication() {
    return std::make_unique<HomeApplication>();
}

}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const bool valid = std::string_view(argv[1]) == "valid";
    auto& shell = platform::runtime::Shell::instance();
    auto& manager = shell.applicationManager();
    assert(manager.registerApplication("calendar", createApplication, platform::runtime::Residency::Transient));
    assert(manager.registerApplication("other", createApplication, platform::runtime::Residency::Transient));
    assert(shell.open("app://other/"));
    assert(!shell.isHome());
    assert(shell.goHome() == valid);
    assert(shell.isHome() == valid);
    if (valid) {
        assert(received.url.applicationName == "calendar");
        assert(received.url.location == "/path/to/page?id=42&mode=week#top");
        assert(enters == 2 && leaves == 1);
        assert(shell.open("app://home"));
        assert(enters == 3 && leaves == 1);
        assert(shell.isHome());
        assert(shell.open("app://calendar/"));
        assert(!shell.isHome());
        assert(shell.open("app://calendar/path/to/page?id=42&mode=week#other"));
        assert(!shell.isHome());
        assert(shell.goHome() && shell.isHome());
    } else {
        assert(received.url.applicationName == "other");
        assert(enters == 1 && leaves == 0);
    }
    manager.leave();
    std::cout << "Configured home entry tests passed\n";
}
