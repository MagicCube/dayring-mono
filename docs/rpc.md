# BLE RPC and clock synchronization

## Ownership and startup

ServiceManager starts `Frontlight → Power → TaskDispatch → BLE → RPC → Time`. Shutdown reverses this order: Time cancels its request and removes handlers, RPC cancels its scheduler task and pending completions, then BLE stops the host before destroying callback storage. No service start waits for a phone.

| Responsibility | Entry point |
| --- | --- |
| Transport interface and bounded packet | `src/platform/rpc/Transport.h` |
| Wire codec and method IDs | `src/platform/rpc/Protocol.h` |
| Request matching, dispatch, timeouts, handlers | `src/platform/rpc/services/RPCService.*` |
| Cross-core queues and session generations | `src/platform/ble/RPCMailbox.h` |
| NimBLE write/notify GATT and host event dispatch | `src/platform/ble/services/BLEService.cpp` |
| RTC and synchronization, timezone state | `src/platform/time/services/TimeService.*`, `ClockSample.h` |
| RTC writes on the application loop | `src/platform/hal/RtcClock.cpp`, `setClockTime` |
| Reusable Apple RPC endpoint | `tools/dayring-cli/Sources/DayringBLE/RPCPeer.swift` |
| Core Bluetooth adapter and automatic clock provider | `BLECentral+RPC.swift`, `ClockSample.swift`, `TimeZoneMonitor.swift` |

NimBLE callbacks only copy packets into bounded queues or transmit on the host event queue. They never run RPC handlers, access the scheduler, or write RTC/I2C. RPCService owns one 10 ms periodic TaskDispatch task, processes at most four inbound packets per execution, and bounds outstanding requests to eight. Handlers and completions execute on the application loop and must be short/nonblocking; received spans are borrowed only for the callback duration. There is no independent TimeSyncService.

The BLE backend remains a single SDK boundary with about 300 lines in its class definition because its GATT definitions, host event, callbacks and stop/join lifetime are coupled. The independent queue and protocol policies are extracted and separately testable.

## GATT and readiness

- Dayring service: `B86E1000-7C65-4DAB-9F21-6A57D2E84010`.
- Existing encrypted bond-status read: `B86E1001-7C65-4DAB-9F21-6A57D2E84010`.
- Central → peripheral RPC: `B86E1002-7C65-4DAB-9F21-6A57D2E84010`, Write With Response, encrypted, 16-byte minimum key.
- Peripheral → central RPC: `B86E1003-7C65-4DAB-9F21-6A57D2E84010`, Notify. Transmission requires an encrypted bonded connection and active subscription.

After bond verification, the central subscribes and sends `hello`. Only after the application handshake does RPCService expose a ready session to TimeService. A successful CCCD write alone is not proof that the peer application is ready to consume the first notification.

Bonded reconnects can deliver encryption and restored-subscription callbacks before CONNECT. BLEService initializes per-link state once per connection handle, preserving a ready session when CONNECT arrives late. Disconnect and host reset invalidate the handle so reuse starts a fresh session. Tests cover all six callback permutations.

Secure reconnection announces Service Changed so bonded Apple devices can invalidate an older firmware's cached GATT layout. BLECentral handles service invalidation by cancelling RPC, resetting characteristic references and rediscovering. No bond erasure is required.

Each BLE session has a new in-memory generation. Disconnect/unsubscribe/reset clears queues and cancels pending RPC requests. Requests and responses from obsolete generations are ignored. No automatic retries are performed by RPC, since arbitrary methods might have side effects. Queue overflow rejects writes/submission; an unsendable notification terminates the connection. Apple RPC hello and write acknowledgments have five-second deadlines. A failed handshake/write or unexpected link loss triggers up to two fresh connections to the same peripheral, with one/two-second backoff. Successful hello resets the budget. Recovery never replays application requests. Session epochs invalidate handshake/timer callbacks before clearing pending requests, so cleanup cannot emit a second misleading handshake-disconnected failure or abort GATT rediscovery. Slow successful acknowledgments (at least one second) are logged.

## Version 1 wire format

One RPC message occupies one GATT value, at most 20 bytes, so the default ATT MTU is sufficient. All integers are little endian.

| Bytes | Meaning |
| --- | --- |
| 0 | `0xD1`: protocol marker/version |
| 1 | 0 request, 1 response, 2 error |
| 2–3 | Nonzero request ID |
| 4–5 | Method ID |
| 6 | Payload length, 0–12 |
| 7 | Reserved, must be zero |
| 8 onward | Exact payload bytes |

Both directions may initiate requests independently. Responses must match both request ID and method. Errors contain exactly one nonzero code: 1 unknown method, 2 invalid payload, 3 busy. Local completion errors also distinguish timeout, disconnected and cancelled. IDs are never reused during an ESP32 RPCService object's lifetime; after 65,535 requests, submission fails until reboot/recreation instead of risking late-response collisions. Apple resets request IDs when its BLE RPC session resets.

This is an initial small-message protocol, not a bulk-transfer or general fragmentation protocol. Larger future methods need a versioned extension. Timezone names use explicit pagination within this bound.

| ID | Method | Contract |
| --- | --- | --- |
| 0 | `hello` | Central sends empty request; device replies empty and enables application RPC readiness |
| 1 | `ping` | Both endpoints echo up to 12 bytes |
| 2 | `clock.get` | Empty request to Apple peer; 8-byte unsigned UTC Unix seconds and 4-byte signed current UTC offset in seconds |
| 3 | `timezone.get` | One-byte byte offset into the clock sample's IANA identifier; response is up to 12 ASCII bytes; fewer than 12 terminates the name |
| 4 | `clock.changed` | Empty request to device; acknowledges a request to refresh time/timezone, not completion of RTC synchronization |
| 5 | `clock.status` | Empty request to device; 12-byte RTC readback: state, year u16, month, day, hour, minute, second, offset i32 |

Clock status states are 0 waiting, 1 pending, 2 synchronized, 3 failed. Custom Swift handlers use IDs above 5. BLECentral exposes `requestRPC`, `cancelRPC`, and `registerRPCHandler`; the standalone RPCPeer is independent of Core Bluetooth and can also be used with another transport.

## Time and timezone policy

TimeService reads RTC independently of BLE. On each ready RPC session it immediately fetches a time sample and matching timezone identifier. It writes the RTC only after validating the whole sample; on success the next sync is eight hours later. A failure retries after 30 seconds while connected. Disconnect cancels the request; reconnect always starts a fresh sync. Request timeouts use monotonic ticks with unsigned wrap-safe elapsed arithmetic. Sync timestamps and timezone metadata live only in RAM, with no NVS writes.

The RTC retains the project's existing local-wall-time convention. Wire data is UTC plus a signed offset; conversion validates years 2000–2099, offsets within ±14 hours, month/year boundaries and weekdays. Timezone names are bounded to 64 ASCII identifier characters. The clock/timezone transaction must complete within five seconds so stale samples are rejected. This is second-resolution BLE synchronization, not precision NTP; transport delay is not compensated.

`timeZone()`, `utcOffsetSeconds()` and `hasTimeZone()` expose the last successfully applied timezone (for example Asia/Shanghai, +28800). Initial timezone knowledge is unavailable until a successful sync. A sync failure preserves the previous applied timezone/time metadata; `syncState()` describes freshness. Future persistence belongs behind this interface, but is deliberately not implemented now.

The Apple peer snapshots the timezone when answering clock.get, and serves timezone.get pages from that same snapshot. It checks the current IANA identifier and UTC offset every ten seconds while active; travel, a zone change with the same offset, or a daylight-saving offset change sends clock.changed immediately on detection. Failed change notifications are retried. TimeService discards an in-flight old sample after a change notification and starts a fresh transaction. iOS background execution is not guaranteed: changes are detected on resume, or fresh state is fetched on reconnect. The ESP32 does not yet bundle an offline timezone-rule database, so future offline DST transitions require that later feature or a fresh connection.

## Verification

- `make test-rpc-service`: codec, dispatch, matching, duplicates, unknown methods, timeout boundaries/wraparound, cancellation, session isolation and queue capacity.
- `make test-time-sync`: immediate/eight-hour/reconnect sync, timezone pagination, calendar conversion, timezone changes during a transaction, malformed responses, RTC failure, timeout and stop.
- `make test-ble-service`: secure subscription gating, actual firmware-branch GATT writes/notifications against simulated NimBLE APIs, session cleanup.
- Swift package tests cover matching wire vectors, bidirectional RPC, timeout/disconnect behavior, clock encoding, timezone travel/DST and retry.
- `make test` covers the integrated service graph and existing UI/runtime behavior. Compile the shared Swift sources for iOS as well.

On hardware, run `./tools/dayring-cli/dayring-cli dev-server`. Require PAIRED, RPC ready, RPC ping succeeded, Clock sample served, and RTC synchronized with a device readback. The CLI retains the connection for periodic sync. The eight-hour boundary is tested with a simulated monotonic clock rather than an eight-hour wall-clock wait.

Physical verification on 2026-09-21 passed on the paired PaperMono and macOS CLI: RPC hello, ping, device-initiated clock/timezone fetch (Asia/Shanghai), and RTC readback `2026-09-21 11:03:35`, UTC offset `28800`. Final firmware was uploaded through the existing preparation hook. No system timezone setting was changed during testing.

### Reconnect ordering regression (2026-09-21)

A physical reconnect trace showed ENC_CHANGE and restored SUBSCRIBE creating a ready session before CONNECT. The old CONNECT handler cleared security, subscription and the session again, despite the encrypted bond-status read succeeding. RPC then timed out or returned insufficient authentication. BLEService now initializes state once per connection handle and invalidates the handle on disconnect/reset. Firmware tests exercise all six permutations of CONNECT, ENC_CHANGE and SUBSCRIBE, plus duplicate CONNECT and reused handles after host reset.

With the fixed firmware uploaded without erasing bonds, a physical probe completed three synchronized sessions: initial connect, recovery after an injected RPC write timeout, and recovery after a link cancellation. Each completed hello, ping and RTC readback. The probe exited successfully and released the connection. RPC deadlines remain five seconds. This is a focused reconnect check, not a multi-hour idle soak test.
