# Dayring CLI

A development-only macOS CLI and reusable Swift Core Bluetooth library. Requires Xcode command-line tools with Swift 5.9 or later and macOS 13+. The library supports iOS 16+; the CLI is macOS-only.

## Run

From the repository root:

```sh
make dev-server
./tools/dayring-cli/dayring-cli
./tools/dayring-cli/dayring-cli --help
./tools/dayring-cli/dayring-cli dev-server --list --timeout 10
./tools/dayring-cli/dayring-cli dev-server
./tools/dayring-cli/dayring-cli dev-server --device 00000000-0000-0000-0000-000000000000
./tools/dayring-cli/dayring-cli dev-server --service B86E1000-7C65-4DAB-9F21-6A57D2E84010
```

Running `./tools/dayring-cli/dayring-cli` without arguments defaults to `dev-server`. Use `--help` or `-h` to display help.

`dev-server` connects to the first connectable advertisement containing the configured service UUID, optionally restricted by the Core Bluetooth peripheral identifier. It then discovers that service, reads the encrypted pairing-status characteristic, verifies the peripheral reports a stored bond, and holds the connection until Ctrl-C, termination, or an unrecoverable Bluetooth failure. RPC handshake/write failures and unexpected disconnections retry the same peripheral up to twice, after one and two seconds. Each retry rediscovers GATT and repeats encrypted bond verification and RPC hello; successful hello resets the retry budget. Outstanding application requests fail instead of being replayed. Use `--device` when multiple development boards advertise the same service. Identifiers are Core Bluetooth UUIDs, not Bluetooth MAC addresses.

`dev-server --list` lists nearby advertisements without connecting, marks recognized advertisements with MATCH, and exits successfully after the scan timeout. Discovery is deduplicated by peripheral identifier. Names and RSSI are diagnostic only. The CLI does not connect based on a matching name.

Startup and scanning each have a bounded `--timeout` (default 20 seconds). Connection, service discovery and subscription each use `--connect-timeout` (default 15 seconds). RPC hello and write acknowledgments retain five-second deadlines. Pairing has a `--pair-timeout` (default 60 seconds). Use `--connect-only` to skip the Dayring-specific pairing check for other test peripherals. Exit codes are 0 for intentional stop/list completion, 1 for Bluetooth failure, scan timeout or exhausted connection recovery, and 2 for argument errors.

macOS may ask for Bluetooth access on the first actual scan. The executable embeds Info.plist with NSBluetoothAlwaysUsageDescription. Enable Bluetooth and grant access to the requesting CLI/terminal host in System Settings > Privacy & Security > Bluetooth if necessary. Help and argument validation do not initialize Bluetooth.

## Ownership and portability

- `DayringBLE/DeviceProfile.swift`: service identity, optional device selection, and typed events.
- `DayringBLE/BLECentral.swift`: owns CBCentralManager, retains the active CBPeripheral, bounds operations, validates service discovery, and cancels work on stop.
- `DayringBLE/BLECentral+Recovery.swift`: cancels the failed connection and schedules bounded reconnects; `ConnectionRecovery.swift` owns the retry budget.
- `DayringCLI/main.swift`: argument parsing, output, signals, process lifetime and exit status.

Create, start and stop BLECentral on the main queue. Its delegate callbacks and onEvent closure run on the main queue. Retain the central for the session, call stop before releasing it, and use weak captures when an owner installs an event handler. Consume events without synchronously restarting the central from a callback; schedule a new session on the main queue instead.

An iOS app can add this directory as a local Swift package and link only the DayringBLE library product. Its own Info.plist must provide NSBluetoothAlwaysUsageDescription. UIKit/SwiftUI, terminal output, process exit, and Unix signals are not dependencies of the library. Automatic reconnect runs only while the foreground process is alive; iOS background restoration is not implemented.

## Initial firmware contract

The development service UUID is **B86E1000-7C65-4DAB-9F21-6A57D2E84010**. Firmware must advertise it in connectable advertising and expose it as a GATT service. BLEService implements this service and the encrypted status characteristic `B86E1001-7C65-4DAB-9F21-6A57D2E84010`. `--service` permits another full 128-bit UUID for experiments.

Recognized advertisement, connected link, verified service, secure pairing, application authorization and RPC readiness are distinct states. The tool verifies an encrypted read and the peripheral-reported stored bond; application authorization is not implemented. RPC starts after subscription and an application-level readiness handshake. An advertised UUID is not authentication. It does not inspect the system bond list or automatically trust ownership.

Central/peripheral roles determine connection initiation, not RPC direction. RPC carries central-to-peripheral frames using characteristic writes and peripheral-to-central frames using notifications. RPCPeer owns framing, request identifiers, responses and deadlines; BLECentral owns the link. Application authorization remains separate. The pairing-status characteristic is independent of the RPC wire format. See [firmware pairing contract](../../docs/ble.md).

## Validation

```sh
swift test --package-path tools/dayring-cli --scratch-path .cache/dayring-cli
make test-rpc-interop test-cli-administration
./tools/dayring-cli/dayring-cli --help
```

Automated tests cover service recognition, optional peripheral selection, bounded retry/backoff, and stale handshake cancellation during stop or GATT rediscovery. Real-radio validation requires a BLE peripheral: verify list-only discovery, service-filtered connect, service validation, timeout, Ctrl-C, remote disconnect, Bluetooth disabled, and permission denial. The user verified Dayring pairing, automatic setup completion, and encrypted reconnection after a device restart with the bond retained. On iOS, also compile the library in the app target and validate permissions on a physical device.

References: [Apple central delegate lifecycle](https://developer.apple.com/documentation/corebluetooth/cbcentralmanagerdelegate), [Bluetooth usage description](https://developer.apple.com/documentation/bundleresources/information-property-list/nsbluetoothalwaysusagedescription).

## RPC and time synchronization

After pairing, the CLI subscribes to RPC notifications, completes hello, verifies ping, and answers device-initiated clock/timezone requests using this computer's current time and IANA timezone. A device RTC readback is printed after the sample is transferred. The device requests time immediately on each ready session and every eight hours after successful synchronization. Keep the CLI running to maintain the connection.

Timezone identifiers and current offsets are monitored every ten seconds while the peer is active. Travel and DST changes notify the device to resynchronize; the paired device does not have to wait eight hours. Timezone settings and synchronization timestamps are currently RAM-only on ESP32. Suspended iOS apps cannot promise continuous background service; reconnect/resume refreshes the current state.

`RPCPeer` provides reusable bidirectional requests, handlers, timeouts and cancellation. `BLECentral.requestRPC`, `cancelRPC`, and `registerRPCHandler` expose it through BLE. Custom method IDs start at 8. Requests and responses each accept up to 10,240 bytes after capability negotiation. Larger messages are automatically fragmented with bounded buffering and per-fragment flow control; short messages retain their single-packet path. The default request deadline is 120 seconds and can be overridden; hello, write acknowledgments and clock sync retain five-second limits. Upgrade firmware and the CLI together. The wire contract, GATT UUIDs, capacity bounds and service ownership are in [RPC and time](../../docs/rpc.md).

## Device administration

Stop `dev-server` first, then use the exact Core Bluetooth UUID shown by `dev-server --list`:

```sh
./tools/dayring-cli/dayring-cli reset-pairing --device DEVICE_UUID
./tools/dayring-cli/dayring-cli reboot --device DEVICE_UUID
./tools/dayring-cli/dayring-cli reset-pairing --device DEVICE_UUID --mac-only
```

`reset-pairing` clears every peer bond stored on the selected ESP32, confirms persisted erasure over RPC, removes only that device's bond from this Mac, and schedules a device restart. It does not erase unrelated Mac pairings or other ESP32 NVS data. macOS removal is preflighted and verified through a capability-checked private pairing-agent interface in the CLI; this is a development utility, not a portable public Core Bluetooth API. If the OS denies the operation, use System Settings > Bluetooth > Forget This Device.

`--mac-only` recovers a partial reset without connecting to ESP32. Use it if the device has already lost its bond but macOS still holds the old keys. Reconnect and pair afresh before retrying the full reset if needed. Reset is not atomic across systems: errors report whether device erasure was confirmed and return exit code 1. No administrative operation is automatically replayed.

`reboot` sends an empty `device.reboot` request (method 7), prints acceptance, and exits without reconnecting. The firmware waits at least one second and for any active e-ink refresh before restarting. Bonds are retained. Acceptance does not prove that a later boot succeeded.

The reset method is `pairing.reset` (6): empty payload starts erasure; `[1]` polls status; `[0]` response is pending, `[1]` is erased, remote error 7 is failure. Polling is scoped to the requesting RPC session. The CLI uses five-second request deadlines and a bounded operation deadline (`--timeout`, default 20 seconds after RPC readiness). A timed-out reset may have completed remotely; do not infer an unchanged device from a lost response.

Automated administration tests use fakes and do not modify this Mac's bonds or reboot a real ESP32. Physical erasure/restart behavior and OS-specific unpairing permissions must be verified separately when deliberately exercising those commands.

## Two-day calendar snapshots

```sh
./tools/dayring-cli/dayring-cli calendar --timeout 60
```

This command requests calendar read permission and writes a compact UTF-8 JSON snapshot to stdout without connecting to Bluetooth. Diagnostics go to stderr. The snapshot contains all non-cancelled events in today and tomorrow's natural-day window, in the Mac system timezone, from all accessible EventKit calendars. Completed, ongoing, and later-today events are included; cancelled events are excluded. Recurring events are already flattened. There is no five-event limit and no implicit declined-invitation filter.

Every event has only `instanceId`, `title`, `location`, `start`, `end`, and `isAllDay`. The envelope includes schema version, generation time, timezone, and coverage bounds. All-day end dates are exclusive. Calendar access requires full event permission on macOS 14+ (legacy access on macOS 13); the CLI does not write calendar data. If denied, grant access to the requesting executable/terminal host in System Settings > Privacy & Security > Calendars and retry.

Normal `dev-server` starts the calendar provider automatically. EventKit queries run off the main queue. Device pull uses `calendar.begin` (8) followed by `calendar.read` (10) for immutable JSON chunks; Mac pushes only empty `calendar.changed` (9) requests after a relevant normalized change. ESP32 CalendarService implements the client and method-9 handler; Lock-screen integration and a physical BLE pull/save-state readback have been verified; Home integration remains pending. Existing firmware can continue clock synchronization and reports unknown-method for calendar notifications, which the CLI suppresses for the rest of that session. After serving the final chunk, the CLI reads `calendar.status` (11) and prints device sync/save state and counts without event contents. See [Calendar contract](../../docs/calendar.md#implemented-cli-contract) for payloads, busy/error handling, retry and safety budgets.

`--help`, `dev-server --list`, `--connect-only`, reset, and reboot do not request calendar access. `make test-cli-calendar` tests synthetic calendars and RPC state without reading personal events or operating hardware.

## Device-requested weather

Normal `dev-server` serves `weather.get` (12). Each ESP32 request temporarily fetches `https://wttr.in/?format=j1`; wttr.in resolves the requesting public IP to a city. No Core Location or location permission is used. VPN/proxy routing can change the returned city.

The ephemeral URLSession disables local caching and returns current WWO condition code, today's minimum and maximum Celsius temperatures, and the API's nearest city name. There is no CLI weather timer, prefetch or retained weather cache. HTTP failure and invalid weather data return RPC failure. Logs label the HTTP stage, its 30-second request / 45-second resource timeouts, and error domain/code.

ESP32 WeatherService owns the hourly pull schedule and the optional report and a daily FATFS cache. On reboot, a date-matching cache is displayed before the connected refresh. The lock screen displays the latest report. See [weather wire contract and cache policy](../../docs/weather.md). Run `make test-cli-weather test-rpc-interop` for synthetic tests without HTTP requests or hardware changes.
