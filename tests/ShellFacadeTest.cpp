#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "platform/hal/Hardware.h"
#include "platform/hal/PowerManager.h"
#include "platform/runtime/Shell.h"
#include "platform/ui/Page.h"

namespace {
using platform::runtime::Application;
using platform::runtime::ApplicationManager;
using platform::runtime::Intent;
using platform::runtime::Shell;
std::vector<std::string> events;
std::vector<std::string> destroyed;
uint8_t brightness = 20;

static_assert(std::is_default_constructible_v<ApplicationManager>);
static_assert(!std::is_default_constructible_v<Shell>);
static_assert(!std::is_copy_constructible_v<Shell>);
static_assert(!std::is_move_constructible_v<Application>);
static_assert(!std::is_copy_constructible_v<platform::ui::Page>);
static_assert(std::is_same_v<decltype(std::declval<const Application&>().owner()), const ApplicationManager*>);
static_assert(std::is_same_v<decltype(std::declval<const platform::ui::Page&>().owner()), const Application*>);

class TestPage final : public platform::ui::Page {
   public:
    explicit TestPage(std::string name) : _name(std::move(name)) {
    }
    void onEnter(std::string_view location) override {
        assert(owner() && owner()->owner());
        assert(!owner()->owner()->open("app://calendar/"));
        events.push_back(_name + ":enter:" + std::string(location));
    }
    void onLeave() override {
        assert(owner() && owner()->owner());
        assert(!owner()->owner()->open("app://calendar/"));
        events.push_back(_name + ":leave");
    }
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }

   private:
    std::string _name;
};

class TestApplication final : public Application {
   public:
    explicit TestApplication(std::string name)
        : root(name + ":root"), detail(name + ":detail"), lockPage(name + ":lock"), _name(std::move(name)) {
        assert(!owner() && !root.owner());
    }
    ~TestApplication() override {
        destroyed.push_back(_name);
    }
    int state = 0;
    TestPage root;
    TestPage detail;
    TestPage lockPage;
    Intent lastIntent;
    void onCreate() override {
        assert(owner());
        assert(router().registerPage("/", root));
        assert(router().registerPage("/detail", detail));
        assert(router().registerPage("/lock", lockPage));
        assert(root.owner() == this && detail.owner() == this);
    }
    void onEnter(const Intent& intent) override {
        assert(!owner()->open("app://calendar/"));
        lastIntent = intent;
        events.push_back(_name + ":enter");
        if (intent.reason == Intent::Reason::Open) {
            assert(navigation().replace(intent.url.location.c_str()));
        } else {
            assert(!navigation().push("/"));
            assert(!navigation().replace("/"));
            assert(!navigation().pop());
        }
    }
    void onLeave() override {
        assert(!owner()->open("app://calendar/"));
        events.push_back(_name + ":leave");
    }
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }

   private:
    std::string _name;
};

TestApplication* calendar = nullptr;
TestApplication* shellApplication = nullptr;
std::unique_ptr<Application> createCalendar() {
    auto application = std::make_unique<TestApplication>("calendar");
    calendar = application.get();
    return application;
}
std::unique_ptr<Application> createShell() {
    auto application = std::make_unique<TestApplication>("shell");
    shellApplication = application.get();
    return application;
}
std::unique_ptr<Application> createAuxiliary() {
    return std::make_unique<TestApplication>("auxiliary");
}
std::unique_ptr<Application> createThird() {
    return std::make_unique<TestApplication>("third");
}
void expect(std::vector<std::string> expected) {
    assert(events == expected);
    events.clear();
}

void testIndependentManagers() {
    {
        ApplicationManager first;
        ApplicationManager second;
        assert(first.registerApplication("calendar", createCalendar, platform::runtime::Residency::Transient));
        assert(second.registerApplication("calendar", createCalendar, platform::runtime::Residency::Transient));
        assert(first.open("app://calendar/"));
        auto* firstApplication = calendar;
        assert(firstApplication->owner() == &first);
        assert(second.open("app://calendar/detail"));
        assert(calendar != firstApplication && calendar->owner() == &second);
        assert(firstApplication->navigation().currentLocation() == "/");
        second.leave();
        assert(firstApplication->navigation().push("/detail?id=1"));
    }
    events.clear();
}

void testCrossApplicationRestoration() {
    auto& shell = Shell::instance();
    auto& manager = shell.applicationManager();
    assert(&shell == &Shell::instance());
    assert(!shell.lock() && !shell.isLocked());
    assert(shell.unlock());
    assert(shell.registerApplication("calendar", createCalendar, platform::runtime::Residency::Transient));
    assert(shell.open("app://calendar/detail?id=42#first"));
    assert(calendar->owner() == &manager);
    assert(calendar->navigation().push("/detail?id=43#second"));
    auto* originalCalendar = calendar;
    calendar->state = 42;
    events.clear();
    assert(!shell.lock());
    assert(brightness == 20);
    expect({});
    assert(shell.registerApplication("shell", createShell, platform::runtime::Residency::Resident));
    assert(shell.lock());
    expect({"calendar:detail:leave", "calendar:leave", "shell:enter", "shell:lock:enter:/lock"});
    assert(shell.isLocked() && brightness == 20);
    assert(shellApplication->owner() == &manager);
    assert(!shell.goHome() && !shell.open("app://calendar/"));
    assert(!manager.open("app://calendar/"));
    manager.leave();
    assert(!shellApplication->navigation().push("/"));
    assert(!shellApplication->navigation().replace("/"));
    assert(!shellApplication->navigation().pop());
    assert(shell.lock());
    expect({});
    assert(shell.unlock());
    expect({"shell:lock:leave", "shell:leave", "calendar:enter", "calendar:detail:enter:/detail?id=43#second"});
    assert(!shell.isLocked() && brightness == 20);
    assert(calendar == originalCalendar && calendar->state == 42);
    assert(calendar->lastIntent.reason == Intent::Reason::Restore);
    assert(calendar->navigation().currentPage() == &calendar->detail);
    assert(calendar->navigation().currentLocation() == "/detail?id=43#second");
    assert(calendar->navigation().canPop());
    assert(calendar->navigation().pop());
    assert(calendar->navigation().currentLocation() == "/detail?id=42#first");
    assert(!shellApplication->navigation().currentPage());
    events.clear();
}

void testShellHistoryRestoration() {
    auto& shell = Shell::instance();
    assert(shell.goHome());
    auto& navigation = shellApplication->navigation();
    assert(navigation.push("/detail?selected=7#item"));
    events.clear();
    assert(shell.open("app://shell/lock?mode=night#clock"));
    expect({"shell:detail:leave", "shell:leave", "shell:enter", "shell:lock:enter:/lock?mode=night#clock"});
    assert(navigation.currentPage() == &shellApplication->lockPage);
    assert(!navigation.canPop());
    assert(shell.unlock());
    expect({"shell:lock:leave", "shell:leave", "shell:enter", "shell:detail:enter:/detail?selected=7#item"});
    assert(navigation.currentLocation() == "/detail?selected=7#item");
    assert(shellApplication->lastIntent.reason == Intent::Reason::Restore);
    assert(shell.unlock());
    expect({});
    assert(navigation.pop());
    assert(navigation.currentLocation() == "/");
    assert(!navigation.canPop());
    events.clear();
}

void testInactiveShellHistory() {
    auto& shell = Shell::instance();
    assert(shellApplication->navigation().push("/detail?keep=1"));
    assert(shell.open("app://calendar/"));
    events.clear();
    assert(shell.lock());
    assert(shell.unlock());
    expect({"calendar:root:leave", "calendar:leave", "shell:enter", "shell:lock:enter:/lock", "shell:lock:leave",
            "shell:leave", "calendar:enter", "calendar:root:enter:/"});
    assert(shellApplication->navigation().currentLocation() == "/detail?keep=1");
    assert(shellApplication->navigation().canPop());
    shell.applicationManager().leave();
    events.clear();
}
void testLockedCacheAndEviction() {
    auto& shell = Shell::instance();
    auto& manager = shell.applicationManager();
    using platform::runtime::Residency;
    assert(manager.registerApplication("auxiliary", createAuxiliary, Residency::Transient));
    assert(manager.registerApplication("third", createThird, Residency::Transient));
    assert(shell.open("app://auxiliary/"));
    assert(shell.open("app://calendar/"));
    calendar->state = 99;
    auto* retained = calendar;
    destroyed.clear();
    for (int i = 0; i < 100; ++i) {
        assert(shell.lock());
        assert(shell.unlock());
        assert(calendar == retained && calendar->state == 99);
    }
    assert(destroyed.empty());
    assert(shell.open("app://third/"));
    assert(destroyed == std::vector<std::string>{"auxiliary"});
    assert(calendar == retained && calendar->state == 99);
    assert(shell.open("app://auxiliary/"));
    assert((destroyed == std::vector<std::string>{"auxiliary", "calendar"}));
    assert(shell.open("app://calendar/"));
    assert(calendar->state == 0 && !calendar->navigation().canPop());
    assert(shellApplication->navigation().currentLocation() == "/detail?keep=1");
    manager.leave();
    events.clear();
}
}  // namespace

namespace platform::hal {
void testFrontlightBrightness(uint8_t percent) {
    brightness = percent;
}
}  // namespace platform::hal

int main() {
    platform::hal::powerManager().begin();
    platform::hal::powerManager().update();
    testIndependentManagers();
    testCrossApplicationRestoration();
    testShellHistoryRestoration();
    testInactiveShellHistory();
    testLockedCacheAndEviction();
    std::cout << "Shell facade, ownership, and restoration tests passed\n";
}
