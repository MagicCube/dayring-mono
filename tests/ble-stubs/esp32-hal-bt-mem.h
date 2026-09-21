#pragma once

// Model the Arduino core startup constructor used by direct NimBLE integrations.
inline bool bleControllerMemoryReserved = false;

__attribute__((constructor)) static void reserveTestBluetoothMemory() {
    bleControllerMemoryReserved = true;
}
