#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "platform/runtime/AppURL.h"
#include "platform/runtime/ApplicationManager.h"
#include "platform/ui/Page.h"
#include "platform/ui/PageController.h"
#include "platform/ui/StaticPageController.h"
#include "stubs/NullCanvas.h"

// ApplicationManager lifecycle tests do not require a hardware-backed render target.

namespace {

using platform::runtime::Application;
using platform::runtime::ApplicationManager;
using platform::runtime::AppURL;
using platform::runtime::InputEvent;
using platform::runtime::Intent;
using platform::runtime::Residency;
std::vector<std::string> events;
int resources = 0;
int controllers = 0;
int views = 0;
int viewsCreated = 0;
int viewsDestroyed = 0;
int states = 0;
int controllersCreated = 0;
int controllersDestroyed = 0;
Intent lastIntent;
std::vector<std::string> expectedShutdownEvents;

static_assert(std::is_default_constructible_v<ApplicationManager>);
static_assert(!std::is_copy_constructible_v<ApplicationManager>);
static_assert(!std::is_move_constructible_v<ApplicationManager>);

struct Resource {
    Resource() {
        ++resources;
    }

    ~Resource() {
        --resources;
    }
};

struct TestProps {};

class OwnedPage final : public platform::ui::Page<TestProps> {
   public:
    OwnedPage() {
        ++views;
        ++viewsCreated;
    }

    ~OwnedPage() override {
        --views;
        ++viewsDestroyed;
    }

    void render(platform::ui::Canvas&, const platform::ui::Rect&, const Props&) const override {
    }
};

struct OwnedState {
    OwnedState() {
        ++states;
    }

    ~OwnedState() {
        --states;
    }
};

class RootPageController final : public platform::ui::PageController {
   public:
    RootPageController() {
        ++controllers;
        ++controllersCreated;
    }

    ~RootPageController() override {
        --controllers;
        ++controllersDestroyed;
    }

    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override {
        _view.render(canvas, bounds, {});
    }

   private:
    OwnedPage _view;
    std::unique_ptr<OwnedState> _state = std::make_unique<OwnedState>();
};

class TestApplication final : public Application {
   public:
    explicit TestApplication(const char* name) : _name(name) {
    }

    ~TestApplication() override {
        _record("destroy");
    }

    void onCreate() override {
        assert(owner());
        assert(!_root.owner());
        assert(router().registerPage("/", _root));
        assert(_root.owner() == this);
        assert(router().registerPage("/static", _static));
        assert(_static.owner() == this);
        _record("create");
    }

    void onEnter(const Intent& intent) override {
        lastIntent = intent;
        _record("enter");
        assert(navigation().replace("/"));
    }

    void onLeave() override {
        _record("leave");
    }

    void update() override {
        _record("update");
    }

    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
        _record("render");
    }

    bool onInput(const InputEvent&) override {
        requestRender();
        _record("input");
        return true;
    }

   private:
    void _record(const char* event) {
        events.push_back(_name + ":" + event);
    }

    RootPageController _root;
    platform::ui::StaticPageController<OwnedPage> _static{true};
    std::string _name;
    std::unique_ptr<Resource> _resource = std::make_unique<Resource>();
};

std::unique_ptr<Application> shellApplication() {
    return std::make_unique<TestApplication>("shell");
}

std::unique_ptr<Application> calendarApplication() {
    return std::make_unique<TestApplication>("calendar");
}

std::unique_ptr<Application> testApplication() {
    return std::make_unique<TestApplication>("test");
}

std::unique_ptr<Application> unavailable() {
    return nullptr;
}

void expect(std::vector<std::string> expected) {
    assert(events == expected);
    assert(controllers == resources && views == resources * 2 && states == resources);
    events.clear();
}

void testURLs() {
    for (const auto url : {"app://shell", "app://shell/", "app://shell/lock", "app://shell/path/to/page?x=1",
                           "app://shell?next=app://calendar/"}) {
        const auto parsed = AppURL::parse(url);
        assert(parsed && parsed->applicationName == "shell");
    }
    for (const auto url : {"", "shell", "https://shell/", "app:/shell", "app://", "app:///lock", "app://?x=1",
                           "app://sh ell/", "app://shell:80/", "app://user@shell/"}) {
        assert(!AppURL::parse(url));
    }
    auto parsed = AppURL::parse(std::string("app://test-app_2/path/to/page?text=a%20b&next=app://shell/lock"));
    assert(parsed && parsed->applicationName == "test-app_2");
    assert(parsed->location == "/path/to/page?text=a%20b&next=app://shell/lock");
    parsed = AppURL::parse("app://shell?next=app://calendar/day");
    assert(parsed && parsed->location == "/?next=app://calendar/day");
    parsed = AppURL::parse("app://shell");
    assert(parsed && parsed->location == "/");
    parsed = AppURL::parse("app://shell#clock");
    assert(parsed && parsed->location == "/#clock");
    parsed = AppURL::parse("app://shell/lock?mode=night#clock");
    assert(parsed && parsed->location == "/lock?mode=night#clock");
    parsed = AppURL::parse("app://shell/lock");
    assert(parsed && parsed->location == "/lock");
}

void testRegistration(ApplicationManager& manager) {
    std::string name = "shell";
    assert(manager.registerApplication(name, shellApplication, platform::runtime::Residency::Resident));
    name = "changed";
    assert(!manager.registerApplication("shell", calendarApplication, platform::runtime::Residency::Resident));
    assert(!manager.registerApplication("", testApplication, platform::runtime::Residency::Transient));
    assert(!manager.registerApplication("bad/name", testApplication, platform::runtime::Residency::Transient));
    assert(!manager.registerApplication("test", nullptr, platform::runtime::Residency::Transient));
    assert(manager.registerApplication("test", testApplication, platform::runtime::Residency::Transient));
    assert(resources == 0);
    assert(manager.open("app://shell/"));
    expect({"shell:create", "shell:enter"});
    assert(!manager.open("app://Shell/"));
    manager.update();
    expect({"shell:update"});
}

void testSwitching(ApplicationManager& manager) {
    assert(manager.registerApplication("shell", shellApplication, Residency::Resident));
    assert(manager.registerApplication("calendar", calendarApplication, Residency::Transient));
    assert(manager.registerApplication("test", testApplication, Residency::Transient));
    assert(manager.open("app://shell/lock"));
    expect({"shell:create", "shell:enter"});
    assert(manager.open("app://calendar/month?year=2026"));
    expect({"calendar:create", "shell:leave", "calendar:enter"});
    assert(lastIntent.url.location == "/month?year=2026");
    assert(manager.open("app://test/"));
    expect({"test:create", "calendar:leave", "test:enter"});
    assert(resources == 3);
    NullCanvas target;
    manager.update();
    assert(manager.onInput({InputEvent::Type::Confirm}));
    assert(manager.render(target, {0, 0, 480, 800}));
    expect({"test:update", "test:input", "test:render"});
    assert(manager.open("app://calendar/day"));
    expect({"test:leave", "calendar:enter"});
    assert(manager.open("app://calendar/other?ignored=1"));
    expect({"calendar:enter"});
    assert(lastIntent.url.location == "/other?ignored=1");
    assert(manager.open("app://shell/"));
    expect({"calendar:leave", "shell:enter"});
    manager.leave();
    expect({"shell:leave"});
    assert(resources == 3);
    manager.leave();
    manager.update();
    assert(!manager.needsRender());
    assert(!manager.onInput({InputEvent::Type::Confirm}));
    expect({});
    assert(manager.open("app://test/"));
    expect({"test:enter"});
    manager.leave();
    expect({"test:leave"});
    // Residency is registration policy, even when the factory is shared.
    assert(manager.registerApplication("retained", testApplication, Residency::Resident));
    assert(manager.open("app://retained/"));
    expect({"test:create", "test:enter"});
    manager.leave();
    expect({"test:leave"});
    assert(resources == 4);
}

void testEviction(ApplicationManager& manager) {
    assert(manager.registerApplication("a", calendarApplication, Residency::Transient));
    assert(manager.registerApplication("b", testApplication, Residency::Transient));
    assert(manager.registerApplication("c", shellApplication, Residency::Transient));
    assert(manager.registerApplication("unavailable", unavailable, Residency::Transient));
    assert(manager.open("app://a/"));
    assert(manager.open("app://b/"));
    expect({"calendar:create", "calendar:enter", "test:create", "calendar:leave", "test:enter"});
    assert(!manager.open("app://unavailable/"));
    assert(!manager.open("app://missing/"));
    expect({});
    assert(resources == 2);
    // Reopening a cached app makes it most recent, without allocating another instance.
    assert(manager.open("app://a/"));
    assert(manager.open("app://a/"));
    expect({"test:leave", "calendar:enter", "calendar:enter"});
    assert(manager.open("app://c/"));
    expect({"shell:create", "calendar:leave", "test:destroy", "shell:enter"});
    assert(resources == 2);
    assert(manager.open("app://b/"));
    expect({"test:create", "shell:leave", "calendar:destroy", "test:enter"});
    assert(resources == 2);
    manager.leave();
    expect({"test:leave"});
}

void testConfiguredLimit() {
    for (const std::size_t limit : {0U, 1U, 3U}) {
        ApplicationManager manager(limit);
        assert(manager.registerApplication("a", calendarApplication, Residency::Transient));
        assert(manager.registerApplication("b", testApplication, Residency::Transient));
        assert(manager.registerApplication("c", shellApplication, Residency::Transient));
        assert(manager.open("app://a/"));
        assert(manager.open("app://b/"));
        assert(manager.open("app://c/"));
        assert(resources == (limit == 3 ? 3 : 1));
        manager.leave();
        events.clear();
    }
    assert(resources == 0);
    events.clear();
}

void testDispatchAndFailedOpen(ApplicationManager& manager) {
    NullCanvas target;
    assert(manager.registerApplication("shell", shellApplication, platform::runtime::Residency::Resident));
    assert(manager.registerApplication("unavailable", unavailable, platform::runtime::Residency::Transient));
    assert(manager.open("app://shell/"));
    expect({"shell:create", "shell:enter"});
    assert(manager.allowsIdleLock());
    assert(manager.needsRender());
    assert(manager.render(target, {0, 0, 480, 800}));
    assert(!manager.needsRender());
    assert(!manager.render(target, {0, 0, 480, 800}));
    expect({"shell:render"});

    assert(!manager.open("app://unknown/"));
    assert(!manager.open("not-an-app-url"));
    assert(!manager.open("app://unavailable/"));
    manager.update();
    assert(manager.onInput({InputEvent::Type::Confirm}));
    assert(manager.needsRender());
    assert(manager.render(target, {0, 0, 480, 800}));
    expect({"shell:update", "shell:input", "shell:render"});
    manager.leave();
    expect({"shell:leave"});
    assert(manager.open("app://shell/"));
    assert(manager.needsRender());
    expect({"shell:enter"});
}

void verifyShutdown() {
    assert(resources == 0);
    assert(controllers == 0 && views == 0 && states == 0);
    assert(controllersCreated == controllersDestroyed);
    assert(viewsCreated == viewsDestroyed);
    if (expectedShutdownEvents.size() > 1 && expectedShutdownEvents.back() == "test:destroy")
        std::sort(events.begin(), events.end());
    assert(events == expectedShutdownEvents);
}

}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string_view scenario = argv[1];
    {
        ApplicationManager manager(2);
        if (scenario == "urls") {
            testURLs();
        } else if (scenario == "registration") {
            testRegistration(manager);
            expectedShutdownEvents = {"shell:leave", "shell:destroy"};
        } else if (scenario == "switching") {
            testSwitching(manager);
            expectedShutdownEvents = {"calendar:destroy", "shell:destroy", "test:destroy", "test:destroy"};
        } else if (scenario == "eviction") {
            testEviction(manager);
            expectedShutdownEvents = {"shell:destroy", "test:destroy"};
        } else if (scenario == "limits") {
            testConfiguredLimit();
        } else if (scenario == "dispatch") {
            testDispatchAndFailedOpen(manager);
            expectedShutdownEvents = {"shell:leave", "shell:destroy"};
        } else {
            assert(false && "Unknown test scenario");
        }
    }
    verifyShutdown();
    std::cout << "ApplicationManager " << scenario << " tests passed\n";
}
