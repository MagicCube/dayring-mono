#include <FreeInkUIDisplayTarget.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>

#include "apps/common/PlaceholderApplication.h"
#include "apps/shell/ShellApplication.h"
#include "apps/shell/components/StatusBar.h"
#include "apps/typography/TypographyApplication.h"
#include "platform/hal/Hardware.h"
#include "platform/hal/PowerManager.h"
#include "platform/hal/RtcClock.h"
#include "platform/runtime/Display.h"
#include "platform/runtime/Input.h"
#include "platform/runtime/Shell.h"
#include "platform/ui/ApplicationContainer.h"
#include "stubs/NullCanvas.h"

namespace {

std::array<uint8_t, 800 * 480 / 8> pixels{};
apps::shell::ShellApplication* shellApplication = nullptr;

platform::runtime::Shell& facade() {
    return platform::runtime::Shell::instance();
}

apps::common::PlaceholderApplication* calendar = nullptr;
apps::common::PlaceholderApplication* testApplication = nullptr;
bool tapPending = false;
bool swipePending = false;
std::array<float, 4> swipePoints{};
bool inputActive = false;
float tapX = 0;
float tapY = 0;
bool powerPressed = false;
bool displayBusy = false;
int refreshCount = 0;
uint8_t frontlightBrightness = 100;
unsigned long nowMs = 0;
bool chargingState = false;
uint8_t batteryPercent = 75;
int batteryReads = 0;
int chargingReads = 0;
Rtc::DateTime clockTime{.hour = 12, .minute = 34};

std::unique_ptr<platform::runtime::Application> createShell() {
    auto instance = std::make_unique<apps::shell::ShellApplication>();
    shellApplication = instance.get();
    return instance;
}

std::unique_ptr<platform::runtime::Application> createCalendar() {
    auto instance = std::make_unique<apps::common::PlaceholderApplication>("Calendar");
    calendar = instance.get();
    return instance;
}

std::unique_ptr<platform::runtime::Application> createTest() {
    auto instance = std::make_unique<apps::common::PlaceholderApplication>("Test");
    testApplication = instance.get();
    return instance;
}

void tick(bool pressed = false) {
    powerPressed = pressed;
    platform::hal::powerManager().update(!pressed && platform::hal::hasInputActivity());
    facade().update();
}

void tap(float x, float y) {
    tapPending = true;
    tapX = x;
    tapY = y;
    tick();
    tapPending = false;
}

template <typename Page>
void checkViewDeterminism(const Page& page, const typename Page::Props& props) {
    freeink::ui::DisplayTarget canvas(pixels.data(), 800, 480, 100, freeink::ui::Orientation::Portrait);
    typename Page::RenderResult result;
    const platform::ui::Rect bounds{0, 0, 480, 800};
    page.render(canvas, bounds, props, result);
    const auto expected = pixels;
    assert(result.interactions.count() == 3);
    result.interactions.setFocusedIndex(0);
    result.interactions.setFlash(1, 0);
    const auto hit = result.interactions.data()[0].rect;
    (void)result.interactions.route({.touchPressed = true, .touchX = hit.x, .touchY = hit.y});
    page.render(canvas, bounds, props, result);
    assert(pixels == expected);
    // The generic View interface must execute the same concrete drawing code.
    const platform::ui::View<typename Page::Props>& view = page;
    view.render(canvas, bounds, props);
    assert(pixels == expected);
}

void testViewDeterminism() {
    checkViewDeterminism(apps::shell::pages::HomePage{}, {});
    checkViewDeterminism(apps::common::LandingPage{}, {.title = "Test", .count = 42});
}

void testCounterBoundariesAndLayoutLifetime() {
    using Type = platform::runtime::InputEvent::Type;
    NullCanvas canvas;
    for (const int value : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        apps::common::LandingPageController controller("Test", value);
        const bool atMaximum = value == std::numeric_limits<int>::max();
        const platform::runtime::InputEvent blocked{Type::TouchRelease, static_cast<int16_t>(atMaximum ? 350 : 100),
                                                    600};
        const platform::runtime::InputEvent allowed{Type::TouchRelease, static_cast<int16_t>(atMaximum ? 100 : 350),
                                                    600};
        controller.onEnter("/");
        assert(!controller.onInput(allowed));
        controller.render(canvas, {0, 0, 480, 800});
        assert(!controller.onInput(blocked));
        assert(controller.onInput(allowed));
        assert(controller.onInput(blocked));
        assert(!controller.onInput(blocked));
        controller.onLeave();
        assert(!controller.onInput(allowed));
    }
}

void testTypographyLaunchAndPaging() {
    using Type = platform::runtime::InputEvent::Type;
    assert(facade().registerApplication(
        "typography",
        []() -> std::unique_ptr<platform::runtime::Application> {
            return std::make_unique<apps::typography::TypographyApplication>();
        },
        platform::runtime::Residency::Transient));
    assert(facade().goHome());
    displayBusy = false;
    tick();
    assert(facade().onInput({Type::TouchRelease, 240, 506}));
    assert(!facade().isHome());
    displayBusy = false;
    tick();
    const auto readingPixels = pixels;
    assert(!facade().onInput({Type::TouchRelease, 50, 300}));
    assert(!facade().onInput({Type::TouchRelease, 240, 300}));
    assert(!facade().onInput({Type::Swipe, 240, 150, 240, 400}));
    assert(facade().onInput({Type::TouchRelease, 430, 300}));
    displayBusy = false;
    tick();
    const auto displayPixels = pixels;
    assert(displayPixels != readingPixels);
    assert(!facade().onInput({Type::TouchRelease, 430, 300}));
    assert(facade().onInput({Type::Swipe, 400, 300, 80, 300}));
    displayBusy = false;
    tick();
    assert(pixels == readingPixels);
    assert(facade().onInput({Type::Swipe, 80, 300, 400, 300}));
    displayBusy = false;
    tick();
    assert(pixels == displayPixels);
    assert(facade().onInput({Type::TouchRelease, 50, 300}));
    displayBusy = false;
    tick();
    assert(pixels == readingPixels);
    assert(facade().onInput({Type::Back}));
    assert(facade().isHome());
}

void testHomeLaunch() {
    auto& manager = facade().applicationManager();
    assert(facade().goHome());
    displayBusy = false;
    tick();
    const auto homePixels = pixels;
    using Type = platform::runtime::InputEvent::Type;
    assert(!manager.onInput({Type::TouchRelease, 10, 10}));
    assert(!manager.onInput({Type::TouchPress, 240, 356}));
    assert(!manager.onInput({Type::TouchRelease, 240, 356}));
    assert(manager.registerApplication("calendar", createCalendar, platform::runtime::Residency::Transient));
    assert(manager.registerApplication("test", createTest, platform::runtime::Residency::Transient));
    // Panel-native taps map into the portrait UI, even during a refresh.
    tap(0.445f, 0.5f);
    assert(calendar && !testApplication);
    assert(calendar->navigation().currentLocation() == "/");
    displayBusy = false;
    tick();
    assert(pixels != homePixels);
    const auto calendarPixels = pixels;
    tick(true);
    assert(facade().isLocked() && frontlightBrightness == 0);
    tick(true);
    assert(!facade().isLocked() && frontlightBrightness == 20);
    displayBusy = false;
    tick();
    assert(pixels == calendarPixels);
    tap(0.54f, 0.5f);
    assert(!manager.onInput({Type::TouchRelease, 240, 356}));
    displayBusy = false;
    tick();
    assert(pixels == homePixels);
    tap(0.555f, 0.5f);
    assert(testApplication && testApplication->navigation().currentLocation() == "/");
    displayBusy = false;
    tick();
    assert(pixels != homePixels && pixels != calendarPixels);
    tap(0.54f, 0.5f);
    displayBusy = false;
    tick();
    assert(pixels == homePixels);
    assert(frontlightBrightness == 20);
}

void testMinuteRefresh() {
    platform::runtime::MinuteClock clock;
    nowMs = 0;
    clockTime = {.hour = 23, .minute = 59, .second = 59};
    assert(clock.update());
    nowMs = 1000;
    clockTime = {.hour = 0, .minute = 0, .second = 0};
    assert(!clock.update());
    assert(clock.time().hour == 23 && clock.time().minute == 59);
    nowMs = 2000;
    clockTime.second = 1;
    assert(clock.update());
    assert(clock.time().hour == 0 && clock.time().minute == 0);
    assert(!clock.update());
    // A delayed loop catches up without requiring an exact second match.
    nowMs = 65000;
    clockTime = {.hour = 0, .minute = 1, .second = 4};
    assert(clock.update());
}

void testStatusRefresh() {
    apps::shell::components::StatusBarController status;
    nowMs = 0;
    clockTime = {.hour = 12, .minute = 0, .second = 1};
    batteryReads = chargingReads = 0;
    assert(status.update());
    assert(batteryReads == 1 && chargingReads == 1);
    chargingState = true;
    nowMs = 999;
    assert(!status.update());
    nowMs = 1000;
    assert(status.update());
    assert(batteryReads == 1 && chargingReads == 2);
    nowMs = 2000;
    assert(!status.update());
    chargingState = false;
    nowMs = 3000;
    assert(status.update());
    batteryPercent = 50;
    nowMs = 179999;
    (void)status.update();
    assert(batteryReads == 1);
    nowMs = 180000;
    assert(status.update());
    assert(batteryReads == 2);
}

class LayoutPageController final : public platform::ui::PageController {
   public:
    platform::ui::Rect bounds{};
    int inputs = 0;

    void render(platform::ui::Canvas&, const platform::ui::Rect& area) override {
        bounds = area;
    }
};

class LayoutApplication final : public platform::runtime::Application {
   public:
    LayoutPageController page;

    bool onInput(const platform::runtime::InputEvent&) override {
        ++page.inputs;
        return true;
    }

    void onCreate() override {
        assert(router().registerPage("/", page));
    }

    void onEnter(const platform::runtime::Intent&) override {
        assert(navigation().replace("/"));
    }

    void onLeave() override {
    }

    void render(platform::ui::Canvas& canvas, const platform::ui::Rect& bounds) override {
        page.render(canvas, bounds);
    }
};

void testApplicationContainer() {
    platform::runtime::ApplicationManager manager;
    assert(manager.registerApplication(
        "layout",
        []() -> std::unique_ptr<platform::runtime::Application> { return std::make_unique<LayoutApplication>(); },
        platform::runtime::Residency::Transient));
    assert(manager.open("app://layout/"));
    auto& page = static_cast<LayoutPageController&>(*manager.currentPageController());
    platform::ui::ApplicationContainer container(manager);
    NullCanvas canvas;
    container.update();
    assert(container.isStatusBarVisible());
    container.render(canvas, {0, 0, 480, 800});
    assert(page.bounds.y == 36 && page.bounds.height == 764);
    assert(!container.needsRender());
    assert(!container.onInput({platform::runtime::InputEvent::Type::TouchRelease, 20, 10}));
    page.setFullscreen(true);
    assert(container.needsRender());
    container.update();
    const int previousBatteryReads = batteryReads;
    const int previousChargingReads = chargingReads;
    nowMs += 240000;
    container.update();
    container.render(canvas, {0, 0, 480, 800});
    assert(!container.isStatusBarVisible());
    assert(page.bounds.y == 0 && page.bounds.height == 800);
    assert(batteryReads == previousBatteryReads && chargingReads == previousChargingReads);
    container.showStatusBar();
    assert(!page.isFullscreen() && container.needsRender());
    container.render(canvas, {0, 0, 480, 800});
    assert(batteryReads == previousBatteryReads + 1 && chargingReads == previousChargingReads + 1);
    assert(page.bounds.y == 36 && page.bounds.height == 764);
    container.hideStatusBar();
    assert(page.isFullscreen() && !container.isStatusBarVisible());
    container.render(canvas, {0, 0, 480, 800});
    assert(!container.needsRender());
    container.hideStatusBar();
    assert(!container.needsRender());
    apps::shell::pages::LockPageController lock;
    assert(lock.isFullscreen());
}

void testBottomGestureCapture() {
    using Type = platform::runtime::InputEvent::Type;
    platform::runtime::ApplicationManager manager;
    assert(manager.registerApplication(
        "layout",
        []() -> std::unique_ptr<platform::runtime::Application> { return std::make_unique<LayoutApplication>(); },
        platform::runtime::Residency::Transient));
    assert(manager.open("app://layout/"));
    auto& page = static_cast<LayoutPageController&>(*manager.currentPageController());
    int homes = 0;
    platform::ui::ApplicationContainer container(manager, [&] { ++homes; });
    NullCanvas canvas;
    const platform::runtime::InputEvent up{Type::Swipe, 250, 650, 250, 748};
    assert(!container.onInput(up));
    container.render(canvas, {10, 20, 480, 800});
    assert(container.onInput(up) && homes == 1 && page.inputs == 0);
    const std::array misses{platform::runtime::InputEvent{Type::Swipe, 250, 650, 250, 747},
                            platform::runtime::InputEvent{Type::Swipe, 350, 780, 250, 780},
                            platform::runtime::InputEvent{Type::Swipe, 250, 810, 250, 780},
                            platform::runtime::InputEvent{Type::Swipe, 350, 680, 250, 780},
                            platform::runtime::InputEvent{Type::Swipe, 250, 700, 250, 820},
                            platform::runtime::InputEvent{Type::Swipe, 9, 700, 9, 780},
                            platform::runtime::InputEvent{Type::TouchRelease, 250, 780}};
    for (const auto& event : misses) assert(container.onInput(event));
    assert(homes == 1 && page.inputs == static_cast<int>(misses.size()));
    page.setFullscreen(true);
    assert(!container.onInput(up) && homes == 1);
    container.render(canvas, {10, 20, 480, 800});
    assert(page.bounds.height == 800);
    assert(container.onInput(up) && homes == 2);
    manager.leave();
    assert(!container.onInput(up) && homes == 2);
}

void testSwipeHomeIntegration() {
    platform::hal::powerManager().notifyActivity();
    auto& shell = facade();
    auto& manager = shell.applicationManager();
    assert(shell.open("app://calendar/"));
    displayBusy = false;
    tick();
    const auto* original = manager.currentPageController();
    // Native horizontal motion becomes an upward swipe in the portrait UI.
    swipePoints = {0.98f, 0.5f, 0.8f, 0.5f};
    swipePending = true;
    tapPending = true;
    tapX = 0.445f;
    tapY = 0.5f;
    tick();
    swipePending = tapPending = false;
    assert(shell.isHome() && manager.currentPageController() != original);
    displayBusy = false;
    tick();
    const auto* home = manager.currentPageController();
    const int refreshes = refreshCount;
    displayBusy = false;
    swipePending = true;
    tick();
    swipePending = false;
    assert(manager.currentPageController() == home && !manager.needsRender() && refreshCount == refreshes);
    assert(shell.open("app://calendar/"));
    displayBusy = false;
    tick();
    original = manager.currentPageController();
    assert(shell.lock());
    displayBusy = false;
    tick();
    const auto* lock = manager.currentPageController();
    swipePending = true;
    tick();
    swipePending = false;
    assert(shell.isLocked() && !shell.isHome() && manager.currentPageController() == lock);
    assert(shell.unlock() && manager.currentPageController() == original);
    assert(shell.goHome());
}

}  // namespace

unsigned long millis() {
    return nowMs;
}

namespace platform::hal {

void testFrontlightBrightness(uint8_t percent) {
    frontlightBrightness = percent;
}

bool hasInputActivity() {
    return inputActive || tapPending || swipePending;
}

bool touchTapped(float& x, float& y) {
    x = tapX;
    y = tapY;
    return tapPending;
}

bool touchSwiped(float& startX, float& startY, float& endX, float& endY) {
    startX = swipePoints[0];
    startY = swipePoints[1];
    endX = swipePoints[2];
    endY = swipePoints[3];
    return swipePending;
}

bool powerButtonPressed() {
    return powerPressed;
}

bool displayReady() {
    return !displayBusy;
}

Framebuffer framebuffer() {
    return {pixels, 800, 480, 100};
}

void refreshDisplay() {
    ++refreshCount;
    displayBusy = true;
}

bool readBatteryPercent(uint8_t& percent) {
    ++batteryReads;
    percent = batteryPercent;
    return true;
}

bool readCharging(bool& charging) {
    ++chargingReads;
    charging = chargingState;
    return true;
}

Rtc::DateTime clockTime() {
    return ::clockTime;
}

}  // namespace platform::hal

int main() {
    platform::hal::powerManager().begin();
    platform::hal::powerManager().update();
    assert(frontlightBrightness == 20);
    auto& manager = facade().applicationManager();
    assert(manager.registerApplication("shell", createShell, platform::runtime::Residency::Resident));
    assert(facade().goHome());
    assert(facade().lock());
    tick();
    assert(refreshCount == 1 && pixels.front() == 0x00);
    assert(!manager.allowsIdleLock());
    assert(frontlightBrightness == 20);
    inputActive = true;
    tick();
    assert(facade().isLocked() && frontlightBrightness == 20);
    nowMs += 9999;
    assert(facade().lock());
    tick();
    assert(frontlightBrightness == 20);
    ++nowMs;
    tick();
    assert(facade().isLocked() && frontlightBrightness == 0);
    inputActive = false;
    assert(refreshCount == 1 && pixels.front() == 0x00);
    assert(std::any_of(pixels.begin(), pixels.end(), [](uint8_t byte) { return byte != 0; }));
    const auto lockPixels = pixels;

    // Input still reaches Shell while the first frame is refreshing.
    tick(true);
    assert(manager.allowsIdleLock());
    assert(frontlightBrightness == 20);
    assert(manager.needsRender() && refreshCount == 1);
    for (int i = 0; i < 5; ++i) tick();
    assert(manager.allowsIdleLock());
    assert(frontlightBrightness == 20);
    displayBusy = false;
    tick();
    assert(refreshCount == 2 && pixels.front() == 0xFF);
    assert(pixels != lockPixels);

    // A second click locks again and enters the real lock page.
    tick(true);
    assert(!manager.allowsIdleLock());
    assert(frontlightBrightness == 0);
    assert(refreshCount == 2);
    displayBusy = false;
    tick();
    assert(refreshCount == 3 && pixels == lockPixels);
    displayBusy = false;
    tick();
    assert(refreshCount == 3);

    // Entering Lock again samples the current RTC immediately.
    tick(true);
    assert(manager.allowsIdleLock());
    assert(frontlightBrightness == 20);
    clockTime.minute = 35;
    tick(true);
    assert(!manager.allowsIdleLock());
    assert(frontlightBrightness == 0);
    displayBusy = false;
    tick();
    assert(pixels.front() == 0x00 && pixels != lockPixels);
    assert(!manager.onInput({platform::runtime::InputEvent::Type::TouchPress, 100, 100}));
    assert(!manager.allowsIdleLock());
    assert(frontlightBrightness == 0);
    assert(!shellApplication->navigation().canPop());
    assert(facade().unlock());
    assert(facade().open("app://shell/"));
    assert(manager.allowsIdleLock());
    assert(frontlightBrightness == 20);
    assert(shellApplication->navigation().currentLocation() == "/");
    assert(!shellApplication->navigation().canPop());
    assert(facade().goHome());
    assert(manager.allowsIdleLock());
    assert(frontlightBrightness == 20);
    assert(shellApplication->navigation().currentLocation() == "/");
    assert(!shellApplication->navigation().canPop());
    assert(facade().open("app://shell/lock?mode=night#clock"));
    assert(!manager.allowsIdleLock());
    assert(frontlightBrightness == 0);
    assert(shellApplication->navigation().currentLocation() == "/lock?mode=night#clock");
    assert(!facade().open("app://shell/missing"));
    assert(facade().unlock());
    assert(facade().open("app://shell/missing"));
    assert(shellApplication->navigation().currentLocation() == "/");
    // Idle timeout locks even while the display is busy.
    displayBusy = true;
    nowMs += 51999;
    tick();
    assert(frontlightBrightness == 20);
    ++nowMs;
    tick();
    assert(frontlightBrightness == 10 && !facade().isLocked());
    nowMs += 8000;
    tick();
    assert(frontlightBrightness == 0 && facade().isLocked());
    assert(!facade().goHome());
    assert(facade().unlock());
    assert(frontlightBrightness == 20);
    inputActive = true;
    tick();
    assert(frontlightBrightness == 20);
    nowMs += 70000;
    tick();
    assert(frontlightBrightness == 20);
    inputActive = false;
    nowMs += 60000;
    tick();
    assert(frontlightBrightness == 0);
    assert(facade().isLocked());
    assert(facade().unlock());
    // After unlocking, a tap still launches its target application.
    testHomeLaunch();
    testMinuteRefresh();
    testStatusRefresh();
    testApplicationContainer();
    testBottomGestureCapture();
    testSwipeHomeIntegration();
    testViewDeterminism();
    testCounterBoundariesAndLayoutLifetime();
    testTypographyLaunchAndPaging();
    std::cout << "Shell power-button, app-launching, and rendering tests passed\n";
}
