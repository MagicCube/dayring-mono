#include <FreeInkUIDisplayTarget.h>

#include <cassert>
#include <iostream>
#include <memory>

#include "../native/HostHardware.h"
#include "platform/hal/Hardware.h"
#include "platform/runtime/ApplicationManager.h"
#include "platform/ui/PageController.h"

namespace {

int entries = 0;

class SamplePageController final : public platform::ui::PageController {
   public:
    bool acceptsLocation(std::string_view location) const override {
        const auto query = platform::runtime::LocationQuery::parse(location);
        return query && query->value("reject") != "yes";
    }

    void onEnter(std::string_view) override {
        ++entries;
    }

    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }
};

class SampleApplication final : public platform::runtime::Application {
   public:
    void onCreate() override {
        assert(router().registerPage("/", _page, "Sample help"));
        assert(router().registerPage("/detail", _detail));
    }

    void onEnter(const platform::runtime::Intent& intent) override {
        if (!navigation().replace(intent.url.location.c_str())) (void)navigation().replace("/");
    }

    void onLeave() override {
    }

    void render(platform::ui::Canvas&, const platform::ui::Rect&) override {
    }

   private:
    SamplePageController _page;
    SamplePageController _detail;
};

void testQuery() {
    using platform::runtime::LocationQuery;
    const auto query =
        LocationQuery::parse("/detail?a=hello%20world&a=second&plus=a+b&empty&encoded=%26%3D#?ignored=1");
    assert(query);
    assert(query->value("a") == "hello world");
    assert(query->value("plus") == "a+b");
    assert(query->value("empty") == "");
    assert(query->value("encoded") == "&=");
    assert(!query->value("ignored"));
    assert(!LocationQuery::parse("/?a=%"));
    assert(!LocationQuery::parse("/?a=%GG"));
    assert(!LocationQuery::parse("/?a=%00"));
    assert(LocationQuery::parse("/#?a=%GG"));
    assert(!platform::runtime::AppURL::parse("app://sample//detail"));
    assert(!platform::runtime::AppURL::parse("app://sample/\n"));
}

void testRoutes() {
    using namespace platform::runtime;
    ApplicationManager manager;
    assert(manager.registerApplication(
        "sample", []() -> std::unique_ptr<Application> { return std::make_unique<SampleApplication>(); },
        Residency::Transient));
    const auto descriptions = manager.describeRoutes();
    assert(descriptions && descriptions->size() == 2);
    assert(descriptions->front().page.help == "Sample help");
    assert(entries == 0 && !manager.currentPageController());
    assert(manager.checkRoute("bad") == RouteError::InvalidURL);
    assert(manager.checkRoute("app://missing/") == RouteError::UnknownApplication);
    assert(manager.checkRoute("app://sample/missing") == RouteError::UnknownPage);
    assert(manager.checkRoute("app://sample/?reject=yes") == RouteError::InvalidParameters);
    assert(manager.checkRoute("app://sample/?a=%ZZ") == RouteError::InvalidParameters);
    assert(manager.open("app://sample/detail?a=42#top", OpenMode::Exact));
    assert(manager.currentURL() == "app://sample/detail?a=42#top");
    assert(!manager.open("app://sample/missing", OpenMode::Exact));
    assert(!manager.open("app://sample/?reject=yes"));
    assert(entries == 1 && manager.currentURL() == "app://sample/detail?a=42#top");
    assert(!manager.describeRoutes());
    assert(manager.open("app://sample/missing"));
    assert(manager.currentURL() == "app://sample/");
}

void testOrientation() {
    preview::configureHardware(12, 34, 75, false);
    const auto frame = platform::hal::framebuffer();
    freeink::ui::DisplayTarget target(frame.pixels.data(), frame.width, frame.height, frame.strideBytes,
                                      freeink::ui::Orientation::Portrait);
    target.setGrayPreview(frame.grayPreview.data());
    target.fill({20, 120, 1, 1}, freeink::ui::Paint::solid(freeink::ui::Color::LightGray));
    assert(target.logicalWidth() == 480 && target.logicalHeight() == 800);
    target.fill({0, 0, 1, 1}, freeink::ui::Paint::solid(freeink::ui::Color::Black));
    target.fill({479, 799, 1, 1}, freeink::ui::Paint::solid(freeink::ui::Color::Black));
    target.fill({17, 123, 1, 1}, freeink::ui::Paint::solid(freeink::ui::Color::Black));
    const auto pixels = preview::portraitPixels();
    assert(pixels[0] == 0 && pixels.back() == 0 && pixels[123 * 480 + 17] == 0);
    assert(pixels[120 * 480 + 20] == 170);
    assert(pixels[1] == 255 && pixels[479] == 255 && pixels[799 * 480] == 255);
}

}  // namespace

int main() {
    testQuery();
    testRoutes();
    testOrientation();
    std::cout << "Preview query, shared route, validation and orientation tests passed\n";
}
