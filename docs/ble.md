# BLE discovery and pairing

## Code map

| Responsibility | Entry point |
| --- | --- |
| Service ownership and startup | `src/platform/runtime/services/ServiceManager.cpp`, `ble()` |
| Peripheral host, advertising, SMP, encrypted status, lifecycle | `src/platform/ble/services/BLEService.cpp` |
| Public state and stored bond count | `src/platform/ble/services/BLEService.h` |
| Firmware service and status UUIDs | `src/platform/ble/BLEProfile.h` |
| Shared Apple central | `tools/ble-central/Sources/DayringBLE/BLECentral.swift` |
| Apple UUIDs and status response | `tools/ble-central/Sources/DayringBLE/DeviceProfile.swift` |
| Development CLI | `tools/ble-central/README.md` |

## Lifecycle

Direct NimBLE integration includes Arduino's `esp32-hal-bt-mem.h`, whose startup constructor marks Bluetooth as in use before `initArduino()`. Without it, Arduino releases controller memory before `setup()`, and later controller initialization can panic.

ServiceManager starts BLEService after TaskDispatchService and before RPCService/TimeService. The service initializes the SDK's existing NimBLE host, registers GATT, waits up to five seconds for host synchronization, and starts connectable advertising named Dayring. It requests SMP security when a central connects. Advertising stops during the single connection and resumes on disconnection. Failed advertising restarts are retried every second through the NimBLE host event queue; unexpected advertising completion also restarts discovery. Host reset suspends retries until synchronization restores advertising. Bonds survive link loss, so the central can reconnect and secure the link with its saved keys. As a peripheral, the device remains connectable indefinitely; the central must initiate reconnection. Link initialization is keyed by connection handle because encryption/subscription restoration can precede CONNECT; late CONNECT preserves those states instead of resetting them. Disconnect and host reset invalidate the handle. State snapshots cross from the host task to the owning loop through atomics; callbacks never access applications or controllers.

Stop synchronously stops the NimBLE host before releasing callback/GATT storage, then deinitializes it without deleting bonds. Unexpected host-stop/deinit failures are fatal rather than permitting callback storage to be freed while still in use. Failed startup cleans up its initialized resources. Ordinary native builds report Unavailable without accessing the Mac radio. Route previews substitute `tools/preview/native/HostBLE.cpp` to expose the CLI-selected Bluetooth state (default connected) without accessing a radio, storing bonds, or opening an RPC session. The backend intentionally keeps its host callbacks and GATT storage together (over the recommended class-size limit) because their lifetimes must agree.

## Pairing contract

- Service UUID: `B86E1000-7C65-4DAB-9F21-6A57D2E84010`.
- Pairing status characteristic: `B86E1001-7C65-4DAB-9F21-6A57D2E84010`, read only, encrypted, minimum 16-byte key.
- Request bonding and LE Secure Connections using Just Works (no display/input confirmation and no MITM authentication). This is a development pairing policy, not application ownership authorization. There is no bounded pairing window yet.
- SDK configuration enables `CONFIG_BT_NIMBLE_NVS_PERSIST` and allows three bonds. NimBLE's store callbacks own key storage in the existing NVS partition. No separate paired Boolean or copy of a key is stored by the application. Store-capacity errors reject new storage instead of evicting an existing bond.
- The status read returns `dayring-bonded-v1` only if this connection is encrypted and bonded and its resolved peer identity appears in the host bond store. Otherwise it returns `pending` on an encrypted link. An unencrypted read is denied by GATT.
- Swift discovers and reads this characteristic, allowing Core Bluetooth to negotiate security. Pending storage is retried within the pairing deadline; success emits `paired` / `PAIRED`. This is a peer-reported bond, not access to Apple's private bond database, and not proof of reboot persistence.
- Existing bonds are not silently replaced on repeat-pairing requests. If one side forgets its keys, both sides need explicit bond removal before pairing again; a device-side bond-management command/UI is not implemented yet. Do not erase all NVS as a routine recovery action.

RPC and clock/timezone synchronization are documented in [RPC](rpc.md). Application authorization, passkey UI, background connection restoration and production pairing-window policy remain follow-up work.

## Validation

```sh
make test-ble-service test-service-manager
swift test --package-path tools/ble-central --scratch-path .cache/ble-central
.pio-core/penv/bin/python -m platformio run -e papermono
```

BLEServiceTest executes the firmware branch against a simulated NimBLE API. It verifies protected reads, exact peer recognition, security failure disconnect, advertising restart, retained bond records, restart and startup rollback. It does not emulate SMP or flash.

For hardware acceptance:

1. Connect the PaperMono by USB and upload using the existing firmware upload hook (`make upload`). Preserve NVS.
2. Run `./tools/ble-central/ble scan --pair-timeout 60`. Accept any macOS pairing prompt.
3. Require `PAIRED`, not merely `Connected` or `Service verified`.
4. Ctrl-C, reconnect, and require `PAIRED` again.
5. Power-cycle the board without erasing flash, reconnect, and require `PAIRED` without repeating first-time pairing. This is the persistence check.
6. Check rejected pairing, remote disconnection, and Bluetooth-off handling. Verify a second unrelated saved bond does not authorize the current connection.

The implementation was compiled for ESP32-S3 and the shared Swift library type-checked for iOS. The user verified first-time pairing on physical hardware: the CLI reported PAIRED and the device automatically entered Home. After restarting the device, it bypassed setup and the CLI again reported PAIRED, confirming retained bonding and encrypted reconnection. Physical iOS testing remains pending.
