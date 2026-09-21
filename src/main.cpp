#include <Arduino.h>

#include "apps/RegisterApplications.h"
#include "platform/hal/Hardware.h"
#include "platform/runtime/Shell.h"

using platform::runtime::Shell;

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup() {
    platform::hal::begin();
    auto& shell = Shell::instance();
    if (!shell.startServices() || !apps::registerApplications(shell) || !shell.openStartupPage()) {
        platform::hal::fatal();
    }
}

void loop() {
    platform::hal::update();
    Shell::instance().update();
    delay(10);
}
