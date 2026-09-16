#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <FreeInkUIDisplayTarget.h>
#include <FreeInkUIInputManager.h>
#include <FrontlightManager.h>
#include <InputManager.h>
#include <LedManager.h>
#include <M5Pm1.h>
#include <PaperMonoBoard.h>
#include <Preferences.h>
#include <Rtc.h>
#include <components/controls/button.h>

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <upload-rtc-config.hpp>

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

namespace {
EInkDisplay display(BoardConfig::ACTIVE.display.sclk, BoardConfig::ACTIVE.display.mosi, BoardConfig::ACTIVE.display.cs,
                    BoardConfig::ACTIVE.display.dc, BoardConfig::ACTIVE.display.rst, BoardConfig::ACTIVE.display.busy);
InputManager input;
Rtc rtc;
FrontlightManager frontlight;
LedManager leds;
enum class DisplayPhase { Clearing, Refreshing, Idle };
DisplayPhase phase = DisplayPhase::Clearing;
int16_t displayedMinute = -1;
uint32_t lastRtcPollAt = 0;
uint32_t rtcPollIntervalMs = 0;
enum class TimeFormat { Hours24, Hours12 };
TimeFormat timeFormat = TimeFormat::Hours24;
TimeFormat displayedFormat = TimeFormat::Hours24;
enum class Theme { Light, Dark };
Theme theme = Theme::Light;
Theme displayedTheme = Theme::Light;
constexpr freeink::ui::ActionId kToggleTimeFormat = 1;
constexpr freeink::ui::ActionId kToggleTheme = 2;
freeink::ui::DeviceContext device;
freeink::ui::InteractionBuffer<2> interactions;

[[noreturn]] void fatal(const char* message) {
    Serial.printf("[fatal] %s\n", message);
    while (true) delay(1000);
}

uint8_t monthFromBuildDate(const char* date) {
    static constexpr const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (uint8_t i = 0; i < 12; ++i) {
        if (strncmp(date, kMonths[i], 3) == 0) return static_cast<uint8_t>(i + 1);
    }
    return 1;
}

uint8_t weekdayFor(uint16_t year, uint8_t month, uint8_t day) {
    static constexpr uint8_t kOffsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    uint16_t y = year;
    if (month < 3) --y;
    return static_cast<uint8_t>((y + y / 4 - y / 100 + y / 400 + kOffsets[month - 1] + day) % 7);
}

Rtc::DateTime buildDateTime() {
    Rtc::DateTime value;
    value.month = monthFromBuildDate(__DATE__);
    value.day = static_cast<uint8_t>((__DATE__[4] == ' ' ? 0 : __DATE__[4] - '0') * 10 + (__DATE__[5] - '0'));
    value.year = static_cast<uint16_t>((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 +
                                       (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0'));
    value.hour = static_cast<uint8_t>((__TIME__[0] - '0') * 10 + (__TIME__[1] - '0'));
    value.minute = static_cast<uint8_t>((__TIME__[3] - '0') * 10 + (__TIME__[4] - '0'));
    value.second = static_cast<uint8_t>((__TIME__[6] - '0') * 10 + (__TIME__[7] - '0'));
    value.weekday = weekdayFor(value.year, value.month, value.day);
    return value;
}

#if PAPERMONO_UPLOAD_RTC_SYNC
Rtc::DateTime uploadRtcDateTime() {
    uint64_t packed = PAPERMONO_UPLOAD_RTC_STAMP;
    Rtc::DateTime value;
    value.second = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.minute = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.hour = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.day = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.month = static_cast<uint8_t>(packed % 100U);
    packed /= 100U;
    value.year = static_cast<uint16_t>(packed);
    value.weekday = weekdayFor(value.year, value.month, value.day);
    return value;
}

bool applyUploadRtcSync(Rtc& rtc) {
    static constexpr const char* kPreferencesNamespace = "paper-mono";
    static constexpr const char* kStampKey = "rtc-stamp";
    static constexpr uint64_t kUploadStamp = PAPERMONO_UPLOAD_RTC_STAMP;

    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, false)) {
        Serial.println("[rtc] upload sync: NVS unavailable");
        return false;
    }

    if (preferences.getULong64(kStampKey, 0) == kUploadStamp) {
        Serial.println("[rtc] upload sync already applied");
        preferences.end();
        return true;
    }

    const Rtc::DateTime target = uploadRtcDateTime();
    Rtc::DateTime readback;
    const bool set = rtc.set(target) && rtc.now(readback);
    const bool remembered = set && preferences.putULong64(kStampKey, kUploadStamp) == sizeof(uint64_t);
    preferences.end();

    Serial.printf("[rtc] upload sync %s: %04u-%02u-%02u %02u:%02u:%02u\n", set && remembered ? "applied" : "failed",
                  target.year, target.month, target.day, target.hour, target.minute, target.second);
    return set && remembered;
}
#endif

void initializeRtc() {
    if (!rtc.begin()) fatal("RTC initialization failed");
#if PAPERMONO_UPLOAD_RTC_SYNC
    if (!applyUploadRtcSync(rtc)) fatal("RTC upload sync failed");
#endif
    Rtc::DateTime now;
    if (!rtc.now(now) && !(rtc.set(buildDateTime()) && rtc.now(now))) fatal("RTC time unavailable");
    Serial.printf("[rtc] %04u-%02u-%02u %02u:%02u:%02u\n", now.year, now.month, now.day, now.hour, now.minute,
                  now.second);
}

void drawTime(const Rtc::DateTime& now) {
    char time[9];
    if (timeFormat == TimeFormat::Hours12) {
        const unsigned hour = now.hour % 12 == 0 ? 12 : now.hour % 12;
        snprintf(time, sizeof(time), "%02u:%02u %s", hour, static_cast<unsigned>(now.minute),
                 now.hour < 12 ? "AM" : "PM");
    } else {
        snprintf(time, sizeof(time), "%02u:%02u", static_cast<unsigned>(now.hour), static_cast<unsigned>(now.minute));
    }
    const auto background = theme == Theme::Light ? freeink::ui::Color::White : freeink::ui::Color::Black;
    const auto foreground = theme == Theme::Light ? freeink::ui::Color::Black : freeink::ui::Color::White;
    display.clearScreen(theme == Theme::Light ? 0xFF : 0x00);
    // Construct only after begin() has allocated the native 800 x 480 buffer.
    // DisplayTarget supplies FreeInk's bundled Noto Sans font automatically.
    freeink::ui::DisplayTarget target(display.getFrameBuffer(), display.getDisplayWidth(), display.getDisplayHeight(),
                                      display.getDisplayWidthBytes(), freeink::ui::Orientation::Portrait);
    target.text({0, 0, target.logicalWidth(), target.logicalHeight()}, time,
                {.align = freeink::ui::TextAlign::Center, .color = foreground});
    device = target.deviceContext();
    const freeink::ui::InputSnapshot noInput{};
    freeink::ui::Frame<2> frame(target, device, noInput, interactions);
    auto buttonStyles = freeink::ui::defaultButtonStyles();
    for (auto* style : {&buttonStyles.normal, &buttonStyles.selected, &buttonStyles.focused, &buttonStyles.active,
                        &buttonStyles.disabled}) {
        style->background = freeink::ui::Paint::solid(background);
        style->foreground = freeink::ui::Paint::solid(foreground);
        style->border = freeink::ui::Paint::solid(foreground);
        style->borderWidth = 2;
    }
    constexpr int16_t padding = 24;
    constexpr int16_t buttonWidth = 200;
    constexpr int16_t buttonHeight = 56;
    const auto buttonY = static_cast<int16_t>(target.logicalHeight() - padding - buttonHeight);
    const auto rightButtonX = static_cast<int16_t>(target.logicalWidth() - padding - buttonWidth);
    freeink::ui::button(frame, {padding, buttonY, buttonWidth, buttonHeight},
                        {.label = timeFormat == TimeFormat::Hours24 ? "Use AM/PM" : "Use 24h",
                         .action = kToggleTimeFormat,
                         .inputMask = freeink::ui::InputTouch,
                         .styles = buttonStyles,
                         .radius = 12});
    freeink::ui::button(frame, {rightButtonX, buttonY, buttonWidth, buttonHeight},
                        {.label = theme == Theme::Light ? "Dark mode" : "Light mode",
                         .action = kToggleTheme,
                         .inputMask = freeink::ui::InputTouch,
                         .styles = buttonStyles,
                         .radius = 12});
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2500);
    Serial.printf("[boot] flash=%u MB psram=%u KB\n", ESP.getFlashChipSize() / (1024 * 1024),
                  ESP.getPsramSize() / 1024);
    if (ESP.getFlashChipSize() != 16U * 1024U * 1024U || !psramFound() || ESP.getPsramSize() < 8U * 1024U * 1024U)
        fatal("PaperMono-Lite requires 16 MB Flash and 8 MB PSRAM");
    if (!freeink::papermono::ensureBooted()) fatal("PaperMono board initialization failed");
    uint8_t buttonConfig = 0;
    if (!freeink::m5pm1::configureAppPowerButton(&buttonConfig))
        fatal("M5PM1 single-click reset disable/readback failed");
    Serial.printf("[boot] M5PM1 BTN_CFG=0x%02X\n", buttonConfig);
    if (!leds.begin()) fatal("RGB LED initialization failed");
    leds.clear();
    frontlight.begin();
    frontlight.off();
    display.begin();
    if (!display.getFrameBuffer() || display.getDisplayWidth() != 800 || display.getDisplayHeight() != 480)
        fatal("Display framebuffer/geometry invalid");
    input.begin();
    initializeRtc();

    // Clear the panel once before displaying the clock.
    display.clearScreen(0xFF);
    Serial.println("[display] startup clear");
    freeink::ui::presentAsync(display, freeink::ui::RefreshHint::Full);
}

void loop() {
    input.update();
    // Route taps even while the panel is busy; the latest choice renders next.
    const auto action = interactions.route(freeink::ui::snapshotFrom(input, device));
    if (action.action == kToggleTimeFormat) {
        timeFormat = timeFormat == TimeFormat::Hours24 ? TimeFormat::Hours12 : TimeFormat::Hours24;
        Serial.printf("[clock] format=%s\n", timeFormat == TimeFormat::Hours24 ? "24h" : "AM/PM");
    }
    if (action.action == kToggleTheme) {
        theme = theme == Theme::Light ? Theme::Dark : Theme::Light;
        Serial.printf("[clock] theme=%s\n", theme == Theme::Light ? "light" : "dark");
    }
    if (display.refreshBusy()) {
        delay(10);
        return;
    }
    if (phase != DisplayPhase::Idle) {
        if (!display.displayCommitted()) fatal("Display refresh did not commit");
        display.runMaintenance();
        phase = DisplayPhase::Idle;
        Serial.println("[display] refresh complete");
    }

    const uint32_t nowMs = millis();
    if (timeFormat != displayedFormat || theme != displayedTheme || nowMs - lastRtcPollAt >= rtcPollIntervalMs) {
        Rtc::DateTime now;
        if (!rtc.now(now)) fatal("RTC time unavailable");
        lastRtcPollAt = nowMs;
        // Sample at most 30 seconds apart, landing on the next RTC minute edge.
        const uint32_t secondsToMinute = 60U - now.second;
        rtcPollIntervalMs = (secondsToMinute < 30U ? secondsToMinute : 30U) * 1000U;
        const int16_t minute = static_cast<int16_t>(now.hour * 60 + now.minute);
        if (minute != displayedMinute || timeFormat != displayedFormat || theme != displayedTheme) {
            drawTime(now);
            displayedMinute = minute;
            displayedFormat = timeFormat;
            displayedTheme = theme;
            phase = DisplayPhase::Refreshing;
            Serial.printf("[display] time %02u:%02u\n", now.hour, now.minute);
            // Always use a full refresh, including whole-screen theme inversion.
            freeink::ui::presentAsync(display, freeink::ui::RefreshHint::Full);
        }
    }
    delay(10);
}
