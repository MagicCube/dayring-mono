#include "Hardware.h"

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <LedManager.h>
#include <M5Pm1.h>
#include <PaperMonoBoard.h>

#include "BootImage.h"
#include "FirmwareUpload.h"
#include "Frontlight.h"
#include "RtcClock.h"

namespace platform::hal {
namespace {

EInkDisplay panel(BoardConfig::ACTIVE.display.sclk, BoardConfig::ACTIVE.display.mosi, BoardConfig::ACTIVE.display.cs,
                  BoardConfig::ACTIVE.display.dc, BoardConfig::ACTIVE.display.rst, BoardConfig::ACTIVE.display.busy);
InputManager buttons;
LedManager leds;
bool refreshPending = false;

void displayBootImage() {
    const auto buffer = framebuffer();
    for (int row = 0; row < buffer.height; ++row) {
        boot::composeRow(row, buffer.pixels.subspan(row * buffer.strideBytes, buffer.strideBytes));
    }
    refreshDisplay();
}

}  // namespace

[[noreturn]] void fatal() {
    while (true) {
        delay(5000);
    }
}

void begin() {
    if (!freeink::papermono::ensureBooted()) fatal();
    // The frontlight shares the EPD supply; enable it before display initialization.
    freeink::papermono::setEpdPower(true);
    beginFrontlight();

    if (ESP.getFlashChipSize() != 16U * 1024U * 1024U || !psramFound() || ESP.getPsramSize() < 8U * 1024U * 1024U)
        fatal();
    if (!freeink::m5pm1::configureAppPowerButton()) fatal();

    panel.begin();
    if (!panel.getFrameBuffer() || panel.getDisplayWidth() != 800 || panel.getDisplayHeight() != 480) fatal();
    displayBootImage();

    if (!leds.begin()) fatal();
    leds.clear();
    buttons.begin();
    initializeRtc();
    beginFirmwareUpload();
}

void update() {
    buttons.update();
    if (refreshPending && !panel.refreshBusy()) {
        if (!panel.displayCommitted()) fatal();
        panel.runMaintenance();
        refreshPending = false;
    }
    pollFirmwareUpload();
}

Framebuffer framebuffer() {
    const auto strideBytes = panel.getDisplayWidthBytes();
    const auto height = panel.getDisplayHeight();
    return {{panel.getFrameBuffer(), static_cast<size_t>(strideBytes) * height},
            panel.getDisplayWidth(),
            height,
            strideBytes};
}

const InputManager& input() {
    return buttons;
}

bool powerButtonPressed() {
    return buttons.wasPressed(InputManager::BTN_POWER);
}

bool touchTapped(float& x, float& y) {
    return buttons.wasTouchTap(x, y);
}

bool touchSwiped(float& startX, float& startY, float& endX, float& endY) {
    return buttons.wasSwipe(startX, startY, endX, endY);
}

bool displayReady() {
    return !refreshPending;
}

void refreshDisplay() {
    panel.displayBufferAsync(EInkDisplay::FULL_REFRESH);
    refreshPending = true;
}

bool readBatteryPercent(uint8_t& percent) {
    uint16_t value = percent;
    if (!BatteryMonitor{}.readPercentageChecked(value)) return false;
    percent = static_cast<uint8_t>(value);
    return true;
}

bool readCharging(bool& charging) {
    const auto status = BatteryMonitor{}.readStatus();
    if (!status.chargingKnown) return false;
    charging = status.charging;
    return true;
}

bool hasInputActivity() {
    if (buttons.isTouchPressed() || buttons.wasTouchActivity()) return true;
    for (uint8_t button = InputManager::BTN_BACK; button < InputManager::BTN_POWER; ++button) {
        if (buttons.isPressed(button) || buttons.wasPressed(button) || buttons.wasReleased(button)) return true;
    }
    return false;
}

}  // namespace platform::hal
