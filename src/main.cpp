#include <Arduino.h>

#include "apps/RegisterApplications.h"
#include "platform/hal/FirmwareUpload.h"
#include "platform/hal/Hardware.h"
#include "platform/runtime/Shell.h"

using platform::runtime::Shell;

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup() {
    platform::hal::begin();
    auto& shell = Shell::instance();
    if (!apps::registerApplications(shell) || !shell.goHome()) {
        platform::hal::fatal();
    }
    platform::hal::beginFirmwareUpload();
}

void loop() {
    platform::hal::update();
    platform::hal::pollFirmwareUpload();
    Shell::instance().update();
    delay(10);
}
