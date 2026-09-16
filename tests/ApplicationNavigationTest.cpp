#include <cassert>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "platform/runtime/ApplicationManager.h"
#include "platform/ui/Page.h"
#include "stubs/NullCanvas.h"

namespace {
using platform::runtime::Application;
using platform::runtime::ApplicationManager;
using platform::runtime::ApplicationNavigation;
using platform::runtime::ApplicationRouter;
using platform::runtime::Intent;
std::vector<std::string> events;
int brokenInstances = 0;

class TestPage final : public platform::ui::Page {
   public:
    ApplicationNavigation* navigation = nullptr;
    bool tryRecursiveNavigation = false;
    void onEnter(std::string_view location) override {
        assert(owner());
        assert(&owner()->navigation() == navigation);
        events.emplace_back("enter:" + std::string(location));
        _checkRecursion();
    }
    void onLeave() override {
        events.emplace_back("leave:" + std::string(navigation->currentLocation()));
        _checkRecursion();
    }
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }

   private:
    void _checkRecursion() {
        if (!tryRecursiveNavigation) return;
        assert(!navigation->push("/"));
        assert(!navigation->replace("/"));
        assert(!navigation->pop());
    }
};

class TestApplication final : public Application {
   public:
    TestPage root;
    TestPage detail;
    void onCreate() override {
        assert(owner());
        root.navigation = &navigation();
        detail.navigation = &navigation();
        assert(router().registerPage("/", root));
        assert(router().registerPage("/detail", detail));
        assert(router().registerPage("/lock", detail));
        assert(!navigation().push("/"));
    }
    void onEnter(const Intent& intent) override {
        // A special test intent lets the application retain its current history.
        if (intent.url.location != "/resume") assert(navigation().replace(intent.url.location.c_str()));
    }
    void onLeave() override {
        assert(!navigation().replace("/"));
        events.emplace_back("application:leave");
    }
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }
};

class BrokenApplication final : public Application {
   public:
    BrokenApplication() {
        ++brokenInstances;
    }
    ~BrokenApplication() override {
        --brokenInstances;
    }
    void onCreate() override {
    }
    void onEnter(const Intent&) override {
        assert(false);
    }
    void onLeave() override {
        assert(false);
    }
    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }
};

TestApplication* application = nullptr;
std::unique_ptr<Application> createApplication() {
    auto instance = std::make_unique<TestApplication>();
    application = instance.get();
    return instance;
}
std::unique_ptr<Application> createOther() {
    return std::make_unique<TestApplication>();
}
std::unique_ptr<Application> createBroken() {
    return std::make_unique<BrokenApplication>();
}
void expect(std::vector<std::string> expected) {
    assert(events == expected);
    events.clear();
}

void testRegistry() {
    TestApplication owner;
    TestApplication otherOwner;
    auto& router = owner.router();
    assert(!owner.owner());
    TestPage page;
    TestPage other;
    for (const auto* path :
         std::initializer_list<const char*>{nullptr, "", "detail", "//other", "/?x=1", "/#hash", "app://other/"}) {
        assert(!router.registerPage(path, page));
        assert(!router.resolve(path));
    }
    assert(!page.owner());
    std::string path = "/detail";
    assert(router.registerPage(path.c_str(), page));
    assert(page.owner() == &owner);
    assert(!otherOwner.router().registerPage("/", page));
    assert(router.registerPage("/alias", page));
    assert(page.owner() == &owner);
    path = "/changed";
    assert(router.resolve("/detail") == &page);
    assert(!router.registerPage("/detail", other));
    assert(router.resolve("/detail") == &page);
    assert(!router.resolve("/Detail"));
    assert(!router.resolve("/detail/"));
    assert(!router.resolve("/missing"));
    assert(router.registerPage("/", other));
    assert(router.resolve("/") == &other);
}

void testEmptyAndFirstEntry(ApplicationManager& manager) {
    assert(manager.registerApplication("test", createApplication, platform::runtime::Residency::Transient));
    assert(manager.open("app://test/resume"));
    auto& navigation = application->navigation();
    assert(!navigation.currentPage());
    assert(navigation.currentLocation().empty());
    assert(!navigation.canPop() && !navigation.pop());
    assert(navigation.push("/detail?id=42#section"));
    assert(navigation.currentPage() == &application->detail);
    assert(!navigation.canPop() && !navigation.pop());
    expect({"enter:/detail?id=42#section"});
    assert(manager.registerApplication("other", createOther, platform::runtime::Residency::Transient));
    assert(manager.open("app://other/detail"));
    expect({"leave:/detail?id=42#section", "application:leave", "enter:/detail"});
    assert(manager.open("app://test/resume"));
    expect({"leave:/detail", "application:leave", "enter:/detail?id=42#section"});
    assert(application->navigation().currentLocation() == "/detail?id=42#section");
    assert(!application->navigation().canPop());
}

void testStackAndParameters() {
    auto& navigation = application->navigation();
    std::string location = "/detail?id=43&next=app://other/#part";
    assert(navigation.push(location.c_str()));
    location = "changed";
    assert(navigation.currentLocation() == "/detail?id=43&next=app://other/#part");
    assert(navigation.canPop());
    expect({"leave:/detail?id=42#section", "enter:/detail?id=43&next=app://other/#part"});
    assert(navigation.pop());
    assert(navigation.currentLocation() == "/detail?id=42#section");
    expect({"leave:/detail?id=43&next=app://other/#part", "enter:/detail?id=42#section"});
    assert(navigation.replace("/"));
    assert(!navigation.canPop());
    expect({"leave:/detail?id=42#section", "enter:/"});
    assert(navigation.push("/"));
    assert(navigation.canPop());
    expect({"leave:/", "enter:/"});
    assert(navigation.replace(navigation.currentLocation().data()));
    expect({"leave:/", "enter:/"});
    assert(navigation.pop());
    assert(!navigation.canPop());
    expect({"leave:/", "enter:/"});
}

void testFailureAndRender(ApplicationManager& manager) {
    auto& navigation = application->navigation();
    NullCanvas target;
    assert(manager.render(target, {0, 0, 480, 800}));
    assert(!manager.needsRender());
    for (const auto* location : std::initializer_list<const char*>{nullptr, "", "detail", "//other/", "app://other/",
                                                                   "https://other/", "/missing"}) {
        assert(!navigation.push(location));
        assert(!navigation.replace(location));
    }
    assert(!navigation.pop());
    assert(navigation.currentLocation() == "/");
    assert(!manager.needsRender());
    assert(manager.registerApplication("broken", createBroken, platform::runtime::Residency::Transient));
    assert(!manager.open("app://broken/"));
    assert(brokenInstances == 0);
    assert(!manager.needsRender());
    expect({});
    application->root.tryRecursiveNavigation = true;
    application->detail.tryRecursiveNavigation = true;
    assert(navigation.push("/detail#fragment?still=fragment"));
    assert(manager.needsRender());
    expect({"leave:/", "enter:/detail#fragment?still=fragment"});
    assert(navigation.pop());
    expect({"leave:/detail#fragment?still=fragment", "enter:/"});
}

void testReactivationAndHome(ApplicationManager& manager) {
    manager.leave();
    expect({"leave:/", "application:leave"});
    manager.leave();
    expect({});
    assert(manager.open("app://test/detail?id=2"));
    expect({"enter:/detail?id=2"});
    assert(manager.open("app://test/detail?id=2"));
    expect({"leave:/detail?id=2", "enter:/detail?id=2"});
    assert(!application->navigation().canPop());
    assert(!manager.registerApplication("home", createOther, platform::runtime::Residency::Transient));
    assert(manager.registerApplication("shell", createOther, platform::runtime::Residency::Resident));
    assert(!manager.open("app://home/detail"));
    expect({});
    assert(manager.open("app://shell/"));
    expect({"leave:/detail?id=2", "application:leave", "enter:/"});
    assert(!manager.open("app://home?source=button#top"));
    expect({});
    assert(manager.open("app://shell/"));
    expect({"leave:/", "enter:/"});
    manager.leave();
    expect({"leave:/", "application:leave"});
}
}  // namespace

int main() {
    ApplicationManager manager(2);
    testRegistry();
    testEmptyAndFirstEntry(manager);
    testStackAndParameters();
    testFailureAndRender(manager);
    testReactivationAndHome(manager);
    std::cout << "Application router and navigation tests passed\n";
}
