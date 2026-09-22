# BLE RPC and clock synchronization

## Ownership and startup

ServiceManager starts `Frontlight → Power → TaskDispatch → BLE → RPC → DeviceControl → Time → Calendar → Weather`. Shutdown reverses this order: Weather cancels its request, Calendar cancels its requests and removes its handler, Time cancels its request and removes handlers, DeviceControl cancels scheduled actions and removes its handlers, RPC cancels its scheduler task and pending completions, then BLE stops the host before destroying callback storage. No service start waits for a phone.

| Responsibility | Entry point |
| --- | --- |
| Transport interface and bounded packet | `src/platform/rpc/Transport.h` |
| Message types, limits and small-message codec | `src/platform/rpc/Protocol.h` |
| Fragment codec, bounded queues, acknowledgments and reassembly | `src/platform/rpc/Fragment.h`, `MessageChannel.*` |
| Request matching, dispatch, timeouts, handlers | `src/platform/rpc/services/RPCService.*` |
| Cross-core queues and session generations | `src/platform/ble/RPCMailbox.h` |
| NimBLE write/notify GATT and host event dispatch | `src/platform/ble/services/BLEService.cpp` |
| Pairing reset and deferred software restart | `src/platform/hal/services/DeviceControlService.*` |
| macOS development-only unpairing | `tools/dayring-cli/Sources/DayringCLI/MacPairingStore.swift` |
| Device-initiated weather pull and daily cache | `src/platform/weather/services/WeatherService.*`, `WeatherReport.*`, `WeatherStorage.*`, `FatWeatherFiles.cpp` |
| macOS on-demand IP-based weather provider | `tools/dayring-cli/Sources/DayringCLI/Weather/` |
| RTC and synchronization, timezone state | `src/platform/time/services/TimeService.*`, `ClockSample.h` |
| RTC writes on the application loop | `src/platform/hal/RtcClock.cpp`, `setClockTime` |
| Reusable Apple RPC endpoint and message channel | `tools/dayring-cli/Sources/DayringBLE/RPCPeer.swift`, `RPCMessageChannel.swift` |
| Core Bluetooth adapter and automatic clock provider | `BLECentral+RPC.swift`, `ClockSample.swift`, `TimeZoneMonitor.swift` |

NimBLE callbacks only copy packets into bounded queues or transmit on the host event queue. They never run RPC handlers, access the scheduler, or write RTC/I2C. RPCService owns one 10 ms periodic TaskDispatch task, processes at most four inbound packets per execution, and bounds outstanding requests to eight. Handlers and completions execute on the application loop and must be short/nonblocking; received spans are borrowed only for the callback duration. There is no independent TimeSyncService.

The BLE backend remains a single SDK boundary with about 400 lines in its class definition because its GATT definitions, host event, callbacks and stop/join lifetime are coupled. The independent queue and protocol policies are extracted and separately testable.

## GATT and readiness

- Dayring service: `B86E1000-7C65-4DAB-9F21-6A57D2E84010`.
- Existing encrypted bond-status read: `B86E1001-7C65-4DAB-9F21-6A57D2E84010`.
- Central → peripheral RPC: `B86E1002-7C65-4DAB-9F21-6A57D2E84010`, Write With Response, encrypted, 16-byte minimum key.
- Peripheral → central RPC: `B86E1003-7C65-4DAB-9F21-6A57D2E84010`, Notify. Transmission requires an encrypted bonded connection and active subscription.

After bond verification, the central subscribes and sends `hello` with the single byte `2`; the device echoes `2` to confirm support for 10 KiB messages. Only after the application handshake does RPCService expose a ready session to TimeService. A successful CCCD write alone is not proof that the peer application is ready to consume the first notification.

Bonded reconnects can deliver encryption and restored-subscription callbacks before CONNECT. BLEService initializes per-link state once per connection handle, preserving a ready session when CONNECT arrives late. Disconnect and host reset invalidate the handle so reuse starts a fresh session. Tests cover all six callback permutations.

Secure reconnection announces Service Changed so bonded Apple devices can invalidate an older firmware's cached GATT layout. BLECentral handles service invalidation by cancelling RPC, resetting characteristic references and rediscovering. No bond erasure is required.

Each BLE session has a new in-memory generation. Disconnect/unsubscribe/reset clears queues and cancels pending RPC requests. Requests and responses from obsolete generations are ignored. Application requests and fragments are never automatically replayed, since arbitrary methods might have side effects. Queue overflow rejects writes/submission; an unsendable notification terminates the connection. Apple RPC hello and write acknowledgments have five-second deadlines. A failed handshake/write or unexpected link loss triggers up to two fresh connections to the same peripheral, with one/two-second backoff. Successful hello resets the budget. Recovery never replays application requests. Session epochs invalidate handshake/timer callbacks before clearing pending requests, so cleanup cannot emit a second misleading handshake-disconnected failure or abort GATT rediscovery. Slow successful acknowledgments (at least one second) are logged.

## Message limits and framing

Requests and successful responses each support **0–10,240 payload bytes**, excluding protocol headers. Both directions have the same limit. Business handlers see a complete payload, never partial fragments. `Message::payload` and `Reply::payload` are vectors sized to the actual message; there is no separate size field or 10 KiB stack array. `RPCService::request` rejects oversized requests with ID zero; Swift completes with `invalidFrame`. Oversized handler results produce remote error 2.

The new central requires hello capability `2` and reports an explicit protocol mismatch with old firmware. New firmware still accepts an empty legacy hello, but limits that session to 12-byte payloads. It does not send fragmented messages to legacy peers. Firmware and the CLI should be updated together for large messages.

### Small messages (D1)

Payloads up to 12 bytes retain the existing single-GATT-value encoding. All integers are little endian.

| Bytes | Meaning |
| --- | --- |
| 0 | `0xD1` |
| 1 | 0 request, 1 response, 2 error |
| 2–3 | Nonzero request ID |
| 4–5 | Method ID |
| 6 | Payload length, 0–12 |
| 7 | Reserved, must be zero |
| 8 onward | Exact payload bytes |

### Fragmented messages (D2)

Payloads of 13–10,240 bytes use application-level fragmentation in both directions, including responses. Each fragment occupies one GATT value. The packet limit is the smaller of the current single-ATT-packet capacity and 244 bytes, with a 20-byte fallback. Firmware obtains the negotiated ATT MTU; Apple uses the `.withoutResponse` size limit even though the actual writes use `.withResponse`, avoiding dependence on GATT long writes.

| Bytes | Meaning |
| --- | --- |
| 0 | `0xD2` |
| 1 | 0 request fragment, 1 response fragment; bit 7 marks an acknowledgment |
| 2–3 | Nonzero request ID |
| 4–5 | Method ID |
| 6–7 | Complete payload length, 13–10,240 |
| 8–9 | Fragment byte offset; acknowledgment contains the next expected offset |
| 10 onward | Nonempty fragment data; absent in acknowledgments |

There is one active fragmented send and one receive assembly per endpoint. The sender waits for an application acknowledgment after each fragment; acknowledgment matching includes kind, ID, method, total length and next offset. Thus simultaneous requests with identical IDs cannot acknowledge each other. Acknowledgments have priority over short messages, and short messages bypass large messages waiting for an acknowledgment. A final fragment acknowledgment confirms receipt, not business-operation completion: the RPC response still determines that outcome.

Malformed lengths, out-of-order fragments and inconsistent metadata fail the session. Nonmatching or stale acknowledgments are ignored. A new offset-zero message replaces an abandoned partial assembly. Cancellation removes unsent request data; already delivered operations cannot be undone. No fragment retransmission or automatic business retry is performed. Disconnect, unsubscribe, reset and service stop release all message buffers.

### Budgets and deadlines

- Outstanding local requests: eight.
- Queued fragmented payloads, including the active send: at most eight messages and **20 KiB total**, allocated by actual length.
- Receive assembly: at most **10 KiB**. Short-message staging: eight packets, plus one reserved acknowledgment.
- Cross-core BLE mailboxes: eight packets in each direction, each with a 244-byte capacity. These remain packet queues, not 10 KiB message queues.
- Queue saturation rejects new local submissions. Before invoking a handler, the endpoint reserves room for a maximum-size reply; if unavailable, it returns `busy` without executing the handler. Requests started inside a handler cannot consume that reservation. An exhausted control path terminates the connection.
- Default RPC deadline: **120 seconds**, including local queueing, request transmission and response reception; callers can supply a shorter or longer deadline. Fragmented responses have a 120-second send deadline. The conservative default accommodates 10 KiB round trips at the minimum MTU; it is not a latency promise.
- Fragment acknowledgment and short-message queue progress deadlines: **five seconds**. A stalled send fails the session. Incomplete receive assemblies expire after five seconds without data. Hello, GATT write acknowledgment and clock synchronization retain their five-second deadlines.

The 30 KiB combined large-message storage budget excludes packet queues, vector/container overhead, temporary completed messages and handler-owned memory. It is not a measured peak heap claim. Small messages are not padded to 10 KiB. Files and payloads beyond the limit need business pagination or a separate transfer protocol.

Responses must match both request ID and method. Errors contain exactly one nonzero code: 1 unknown method, 2 invalid payload, 3 busy, 7 internal operation failure. Local completion errors also distinguish timeout, disconnected and cancelled. IDs are never reused during an ESP32 RPCService object's lifetime; after 65,535 requests, submission fails until reboot/recreation instead of risking late-response collisions. Apple resets request IDs when its BLE RPC session resets.

| ID | Method | Contract |
| --- | --- | --- |
| 0 | `hello` | Central sends capability byte `2`; device echoes it and enables 10 KiB RPC (empty legacy hello remains supported) |
| 1 | `ping` | Both endpoints echo up to 10,240 bytes after capability negotiation |
| 2 | `clock.get` | Empty request to Apple peer; 8-byte unsigned UTC Unix seconds and 4-byte signed current UTC offset in seconds |
| 3 | `timezone.get` | One-byte byte offset into the clock sample's IANA identifier; response is up to 12 ASCII bytes; fewer than 12 terminates the name |
| 4 | `clock.changed` | Empty request to device; acknowledges a request to refresh time/timezone, not completion of RTC synchronization |
| 5 | `clock.status` | Empty request to device; 12-byte RTC readback: state, year u16, month, day, hour, minute, second, offset i32 |
| 6 | `pairing.reset` | Empty request starts asynchronous BLE-store erasure; `[1]` polls the same session. Reply `[0]` means pending, `[1]` means persisted keys/subscriptions cleared; error 7 means failure |
| 7 | `device.reboot` | Empty request schedules software restart; empty reply acknowledges scheduling, not completion |

Calendar CLI methods 8 (`calendar.begin`), 9 (`calendar.changed`), 10 (`calendar.read`), and 11 (`calendar.status`) are assigned by the [calendar contract](calendar.md#implemented-cli-contract). The macOS CLI serves snapshots and the ESP32 CalendarService implements the pull client and method-9 handler. A physical real-EventKit pull and saved-state readback passed on 2026-09-21; see calendar verification details. Begin/read use business-level snapshot paging on top of D2 fragmentation. Method 12 (`weather.get`) is assigned to the [weather contract](weather.md). Do not reuse IDs 8–12 for other application handlers.

Clock status states are 0 waiting, 1 pending, 2 synchronized, 3 failed. Custom Swift handlers use IDs above 7. BLECentral exposes `requestRPC`, `cancelRPC`, and `registerRPCHandler`; the standalone RPCPeer is independent of Core Bluetooth and can also be used with another transport.

## Device administration

DeviceControlService registers methods 6 and 7 after RPC startup and unregisters them before RPC shutdown. Requests require the existing encrypted/bonded transport and application handshake. Malformed payloads are rejected before any action. Concurrent resets/reboots return busy. Failed clear attempts can be explicitly retried; neither CLI nor firmware silently replays an administrative action.

`pairing.reset` clears the entire NimBLE store on the host event queue (the bundled SDK includes security keys, CCCDs, client features, peer-address records and the local IRK) and verifies that local security, peer security and CCCD records are empty. It preserves the current encrypted session briefly so the CLI can poll the result. DeviceControlService normally schedules restart when the requester polls the final erased status. If the requester disappears or does not poll within five seconds of completion, it still schedules restart to finish cleanup. On disconnect, the host clears the store again to remove any subscription state written while closing the old link, and suppresses advertising until restart. Other NVS namespaces, RTC data and hardware identity are not factory-reset. The restart discards volatile keys and resolving-list state.

Both reset-triggered and explicit reboots have a one-second response grace period and wait for `hal::displayReady()`. They execute on the application loop through `hal::restartDevice()`, not inside a NimBLE callback or a page. A success response to `device.reboot` acknowledges the scheduled action; it cannot prove the subsequent boot succeeded. Display refresh can extend the restart delay. Service shutdown cancels a scheduled restart.

The CLI commands are `reset-pairing --device UUID` and `reboot --device UUID`. Reset preflights an exact-UUID macOS pairing-agent adapter before requesting erasure, waits for firmware confirmation, stops its BLE connection/recovery, calls macOS unpair, and verifies the bond is no longer present. macOS unpairing relies on private selectors and may be unavailable on another OS version or under another permission policy. The adapter is only in the macOS executable, with selector/signature checks and a verification deadline; it is never linked into the iOS library. It does not delete system-wide preferences or affect unrelated peripherals.

The operation is not atomic across devices. If the response is lost or local removal fails, the CLI exits nonzero and reports what is known. `reset-pairing --device UUID --mac-only` removes only the selected Mac bond without contacting the device, allowing recovery when the ESP32 no longer accepts the old keys. An unsupported adapter requires System Settings > Bluetooth > Forget This Device. Stop other development centrals before running these commands to avoid reconnecting/re-pairing during reset.

## Time and timezone policy

TimeService reads RTC independently of BLE. On each ready RPC session it immediately fetches a time sample and matching timezone identifier. It writes the RTC only after validating the whole sample; on success the next sync is eight hours later. A failure retries after 30 seconds while connected. Disconnect cancels the request; reconnect always starts a fresh sync. Request timeouts use monotonic ticks with unsigned wrap-safe elapsed arithmetic. Sync timestamps and timezone metadata live only in RAM, with no NVS writes.

The RTC retains the project's existing local-wall-time convention. Wire data is UTC plus a signed offset; conversion validates years 2000–2099, offsets within ±14 hours, month/year boundaries and weekdays. Timezone names are bounded to 64 ASCII identifier characters. The clock/timezone transaction must complete within five seconds so stale samples are rejected. This is second-resolution BLE synchronization, not precision NTP; transport delay is not compensated.

`timeZone()`, `utcOffsetSeconds()` and `hasTimeZone()` expose the last successfully applied timezone (for example Asia/Shanghai, +28800). Initial timezone knowledge is unavailable until a successful sync. A sync failure preserves the previous applied timezone/time metadata; `syncState()` describes freshness. Future persistence belongs behind this interface, but is deliberately not implemented now.

The Apple peer snapshots the timezone when answering clock.get, and serves timezone.get pages from that same snapshot. It checks the current IANA identifier and UTC offset every ten seconds while active; travel, a zone change with the same offset, or a daylight-saving offset change sends clock.changed immediately on detection. Failed change notifications are retried. TimeService discards an in-flight old sample after a change notification and starts a fresh transaction. iOS background execution is not guaranteed: changes are detected on resume, or fresh state is fetched on reconnect. The ESP32 does not yet bundle an offline timezone-rule database, so future offline DST transitions require that later feature or a fresh connection.

## Verification

- `make test-rpc-service`: codec, dispatch, matching, duplicates, unknown methods, timeout boundaries/wraparound, cancellation, session isolation, capability gating and payload bounds.
- `make test-rpc-message-channel`: simultaneous 10 KiB requests/responses at 20/64/244-byte packet limits, boundary sizes, queue budget, backpressure, acknowledgment matching, short-message bypass and malformed/expired assemblies.
- `make test-rpc-interop`: runs the actual Swift RPCPeer against C++ RPCService through a pipe transport, with simultaneous 10 KiB requests and 10 KiB returns using identical request IDs. Also checks Swift bounds, cancellation, reset, stalled sends and malformed fragments. Requires Swift and a host C++ compiler, but not XCTest or a radio.
- `make test-device-control`: administrative method validation, clear failure/retry, restart grace/rollover, display gating and stop/unregistration. The test intentionally keeps the sequential lifecycle scenario together so service state transitions remain visible.
- `make test-cli-administration`: command targeting, macOS preflight, reset sequencing, partial outcomes, verification timeout, Mac-only recovery and reboot acknowledgment using fakes.
- `make test-time-sync`: immediate/eight-hour/reconnect sync, timezone pagination, calendar conversion, timezone changes during a transaction, malformed responses, RTC failure, timeout and stop.
- `make test-ble-service`: secure subscription gating, actual firmware-branch GATT writes/notifications against simulated NimBLE APIs, session cleanup.
- Swift package tests cover matching wire vectors, bidirectional RPC, timeout/disconnect behavior, clock encoding, timezone travel/DST and retry.
- `make test` covers the integrated service graph and existing UI/runtime behavior. Compile the shared Swift sources for iOS as well.

On hardware, run `./tools/dayring-cli/dayring-cli dev-server`. Require PAIRED, RPC ready, RPC ping succeeded, Clock sample served, and RTC synchronized with a device readback. The CLI retains the connection for periodic sync. The eight-hour boundary is tested with a simulated monotonic clock rather than an eight-hour wall-clock wait.

Physical verification on 2026-09-21 passed on the paired PaperMono and macOS CLI: RPC hello, ping, device-initiated clock/timezone fetch (Asia/Shanghai), and RTC readback `2026-09-21 11:03:35`, UTC offset `28800`. Final firmware was uploaded through the existing preparation hook. No system timezone setting was changed during testing.

### Reconnect ordering regression (2026-09-21)

A physical reconnect trace showed ENC_CHANGE and restored SUBSCRIBE creating a ready session before CONNECT. The old CONNECT handler cleared security, subscription and the session again, despite the encrypted bond-status read succeeding. RPC then timed out or returned insufficient authentication. BLEService now initializes state once per connection handle and invalidates the handle on disconnect/reset. Firmware tests exercise all six permutations of CONNECT, ENC_CHANGE and SUBSCRIBE, plus duplicate CONNECT and reused handles after host reset.

With the fixed firmware uploaded without erasing bonds, a physical probe completed three synchronized sessions: initial connect, recovery after an injected RPC write timeout, and recovery after a link cancellation. Each completed hello, ping and RTC readback. The probe exited successfully and released the connection. RPC deadlines remain five seconds. This is a focused reconnect check, not a multi-hour idle soak test.

### 10 KiB protocol verification

The D2 extension is covered by host C++ tests and executable Swift/C++ interoperability checks. The macOS Command Line Tools installation used for development lacks XCTest, so the original Swift XCTest suite cannot run in that environment; the standalone interoperability target remains runnable. Earlier physical verification above used the D1 protocol and does not establish D2 throughput or peak device heap. Large-message radio timing, peak heap and UI responsiveness still need a physical-device run.

## Asynchronous Apple handlers

`RPCPeer.registerAsync` and `BLECentral.registerAsyncRPCHandler` allow a handler to complete later on the owning main queue. Weather uses this for location and HTTPS work without blocking clock/calendar RPC traffic. At most eight deferred replies are pending, each with a 120-second deadline. Completion is accepted once; reset and request tokens discard obsolete callbacks even when a new session reuses an ID. A timeout returns internal failure (7). Reply capacity is checked before dispatch and again at completion; deferred work does not reserve the message queue for its lifetime, and saturation returns busy (3). The handler must bound/cancel its own external work.
