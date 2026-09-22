#include <Arduino.h>

#include "apps/RegisterApplications.h"
#include "platform/fonts/Fonts.h"
#include "platform/hal/Hardware.h"
#include "platform/runtime/Shell.h"

using platform::runtime::Shell;

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

namespace {

bool fontsReady = false;

}

void setup() {
    platform::hal::begin();
    fontsReady = platform::fonts::loadFonts();
    if (!fontsReady) return;  // Keep servicing USB updates so missing assets can be provisioned.
    auto& shell = Shell::instance();
    if (!shell.startServices() || !apps::registerApplications(shell) || !shell.openStartupPage()) {
        platform::hal::fatal();
    }
}

void loop() {
    platform::hal::update();
    if (fontsReady) Shell::instance().update();
    delay(10);
}
