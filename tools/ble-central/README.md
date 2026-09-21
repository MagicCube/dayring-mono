# Dayring BLE Central

A development-only macOS CLI and reusable Swift Core Bluetooth library. Requires Xcode command-line tools with Swift 5.9 or later and macOS 13+. The library supports iOS 16+; the CLI is macOS-only.

## Run

From the repository root:

```sh
./tools/ble-central/ble --help
./tools/ble-central/ble scan --list --timeout 10
./tools/ble-central/ble scan
./tools/ble-central/ble scan --device 00000000-0000-0000-0000-000000000000
./tools/ble-central/ble scan --service B86E1000-7C65-4DAB-9F21-6A57D2E84010
```

`scan` connects to the first connectable advertisement containing the configured service UUID, optionally restricted by the Core Bluetooth peripheral identifier. It then discovers that service, reads the encrypted pairing-status characteristic, verifies the peripheral reports a stored bond, and holds the connection until Ctrl-C, termination, Bluetooth failure, or disconnection. It does not retry automatically. Use `--device` when multiple development boards advertise the same service. Identifiers are Core Bluetooth UUIDs, not Bluetooth MAC addresses.

`scan --list` lists nearby advertisements without connecting, marks recognized advertisements with MATCH, and exits successfully after the scan timeout. Discovery is deduplicated by peripheral identifier. Names and RSSI are diagnostic only. The CLI does not connect based on a matching name.

Startup and scanning each have a bounded `--timeout` (default 20 seconds). Connection and service discovery each have a bounded `--connect-timeout` (default 15 seconds). Pairing has a `--pair-timeout` (default 60 seconds). Use `--connect-only` to skip the Dayring-specific pairing check for other test peripherals. Exit codes are 0 for intentional stop/list completion, 1 for Bluetooth failure, scan timeout or remote disconnection, and 2 for argument errors.

macOS may ask for Bluetooth access on the first actual scan. The executable embeds Info.plist with NSBluetoothAlwaysUsageDescription. Enable Bluetooth and grant access to the requesting CLI/terminal host in System Settings > Privacy & Security > Bluetooth if necessary. Help and argument validation do not initialize Bluetooth.

## Ownership and portability

- `DayringBLE/DeviceProfile.swift`: service identity, optional device selection, and typed events.
- `DayringBLE/BLECentral.swift`: owns CBCentralManager, retains the active CBPeripheral, bounds operations, validates service discovery, and cancels work on stop.
- `DayringCLI/main.swift`: argument parsing, output, signals, process lifetime and exit status.

Create, start and stop BLECentral on the main queue. Its delegate callbacks and onEvent closure run on the main queue. Retain the central for the session, call stop before releasing it, and use weak captures when an owner installs an event handler. Consume events without synchronously restarting the central from a callback; schedule a new session on the main queue instead.

An iOS app can add this directory as a local Swift package and link only the DayringBLE library product. Its own Info.plist must provide NSBluetoothAlwaysUsageDescription. UIKit/SwiftUI, terminal output, process exit, and Unix signals are not dependencies of the library. Background restoration and reconnect policy are future work, not guaranteed by this foreground implementation.

## Initial firmware contract

The development service UUID is **B86E1000-7C65-4DAB-9F21-6A57D2E84010**. Firmware must advertise it in connectable advertising and expose it as a GATT service. BLEService implements this service and the encrypted status characteristic `B86E1001-7C65-4DAB-9F21-6A57D2E84010`. `--service` permits another full 128-bit UUID for experiments.

Recognized advertisement, connected link, verified service, secure pairing, application authorization and RPC readiness are distinct states. The tool verifies an encrypted read and the peripheral-reported stored bond; application authorization and RPC are not implemented. An advertised UUID is not authentication. It does not inspect the system bond list or automatically trust ownership.

Central/peripheral roles determine connection initiation, not RPC direction. The next stage can carry central-to-peripheral frames using characteristic writes and peripheral-to-central frames using notifications. A separate RPC layer should own framing, request identifiers, responses, deadlines and authorization; BLECentral should own the link. The pairing-status characteristic is independent of the future RPC wire format. See [firmware pairing contract](../../docs/ble.md).

## Validation

```sh
swift test --package-path tools/ble-central --scratch-path .cache/ble-central
./tools/ble-central/ble --help
```

Automated tests cover service recognition and optional peripheral selection. Real-radio validation requires a BLE peripheral: verify list-only discovery, service-filtered connect, service validation, timeout, Ctrl-C, remote disconnect, Bluetooth disabled, and permission denial. The user verified Dayring pairing, automatic setup completion, and encrypted reconnection after a device restart with the bond retained. On iOS, also compile the library in the app target and validate permissions on a physical device.

References: [Apple central delegate lifecycle](https://developer.apple.com/documentation/corebluetooth/cbcentralmanagerdelegate), [Bluetooth usage description](https://developer.apple.com/documentation/bundleresources/information-property-list/nsbluetoothalwaysusagedescription).
