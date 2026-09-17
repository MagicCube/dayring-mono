#include "FirmwareUpload.h"

#include <Arduino.h>

#include <array>
#include <string_view>

#include "../runtime/Shell.h"

namespace platform::hal {
namespace {

std::array<char, 64> command{};
size_t commandSize = 0;
bool overflow = false;
bool waitingForDisplay = false;

void processCommand() {
    if (overflow || std::string_view(command.data(), commandSize) != "DAYRING PREPARE_UPDATE") return;
    if (runtime::Shell::instance().prepareFirmwareUpdate()) {
        Serial.println("DAYRING PREPARING");
        waitingForDisplay = true;
    }
}

}  // namespace

void beginFirmwareUpload() {
    Serial.begin(115200);
}

void pollFirmwareUpload() {
    // Bound work per loop so serial traffic cannot starve display completion.
    for (size_t count = 0; count < command.size() && Serial.available(); ++count) {
        const char value = static_cast<char>(Serial.read());
        if (value == '\n') {
            processCommand();
            commandSize = 0;
            overflow = false;
        } else if (value != '\r') {
            if (commandSize < command.size())
                command[commandSize++] = value;
            else
                overflow = true;
        }
    }
    if (waitingForDisplay && runtime::Shell::instance().isFirmwareUpdateReady()) {
        Serial.println("DAYRING READY");
        waitingForDisplay = false;
    }
}

}  // namespace platform::hal
