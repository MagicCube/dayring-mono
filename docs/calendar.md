# Calendar synchronization

Status: macOS CLI and ESP32 CalendarService implemented. Lock-screen preview integration is implemented; Home integration and physical power-cut testing remain pending.

## Current product contract

The latest user correction restores today/tomorrow coverage and supersedes the temporary yesterday/today window. Completed events remain included; the upcoming-only filter is not restored. This service returns **all non-cancelled occurrences overlapping today and tomorrow as two natural calendar days**, including already-ended events. It does not filter by whether an event has ended relative to the current time. A consumer may select upcoming rows independently, but that selection must not change the synchronized data.

- Use all event calendars accessible through macOS EventKit; no calendar-selection UI is required.
- Follow the Mac system timezone. For September 21, the interval is `[September 21 00:00, September 23 00:00)`. Compute calendar days, not a rolling 48-hour duration.
- macOS expands recurrence instances. A daily meeting yields distinct today/tomorrow occurrences, each with its own `instanceId`.
- Include source statuses `confirmed`, `tentative`, and `unknown`; exclude `cancelled`. Do not transmit status. Declined participation is not separately filtered because that policy was never confirmed.
- Each occurrence contains exactly `instanceId`, `title`, `location`, `start`, `end`, and `isAllDay`. No calendar ID, recurrence flag, status, or participants are included.
- Neither day has a product-level count cap. Resource exhaustion is an explicit failure, never silent truncation.
- Event data always flows through ESP32-initiated pulls. macOS pushes only an empty "relevant events changed" notification.
- ESP32 pulls on application-ready connection/reconnection and receipt of the change notification. Daily rollover and invalidating time-context changes also require a pull to maintain the window.
- ESP32 retains the complete snapshot in the existing FATFS storage partition and exposes local upcoming queries and scoped change subscriptions through CalendarService. LockPageController consumes upcoming data and its change subscription.
- Broader calendar browsing and the future Calendar application are out of scope.

## Time and identity

Include positive-duration events when `start < windowEnd && end > windowStart`. Preserve original endpoints for overnight/multi-day events; do not clip them. Include zero-duration events when `windowStart <= start < windowEnd`. Reject invalid negative durations. Store a cross-day occurrence once, even though it overlaps both days.

`start` and `end` are ISO 8601 strings containing date, time, and the endpoint-specific UTC offset. A DST transition can give the endpoints different offsets. All-day events use local-midnight boundaries with an exclusive end: one all-day event on September 21 in Shanghai is `[2026-09-21T00:00:00+08:00, 2026-09-22T00:00:00+08:00)`. Interpret these as calendar dates when `isAllDay` is true. Some subscribed calendars return an inclusive last-second all-day end; normalize any non-midnight all-day end to the following local midnight. Preserve an already-exclusive midnight end. EventKit returns floating/all-day dates in the system default timezone, as documented in the SDK's `EKEvent.startDate` contract.

The snapshot envelope contains `schemaVersion: 1`, `generatedAt`, IANA `timeZone`, `windowStart`, `windowEndExclusive`, and `events`. Empty titles/locations are empty strings. Event arrays use start-instant ordering with an identity tie-breaker. Display-specific priority for all-day or ongoing events belongs to the consumer.

IDs are SHA-256 digests of length-delimited source calendar/item/original-occurrence components. Non-recurring items use a fixed occurrence anchor. The original recurrence anchor distinguishes repeated and detached occurrences. IDs are opaque to ESP32 and unique within a snapshot; provider identifier resets may change them. Do not merge different calendars' entries solely because their titles/times match.

Current event endpoint conversion does not solve an offline local-wall-time RTC crossing DST. TimeService remains the sole current-time owner; its future transition policy must be designed before promising offline clock correctness. No second RTC policy belongs in CalendarService.

## Implemented CLI contract

Run `./tools/dayring-cli/dayring-cli calendar --timeout 60` to print a compact UTF-8 JSON snapshot without Bluetooth. Normal `dev-server` provides the same data through RPC and monitors EventKit. Help, list-only discovery, connect-only mode, and administrative commands do not request calendar permission.

macOS 14+ requests full event access; macOS 13 uses legacy event authorization. The CLI does not modify calendars. Permission failures are errors, never a valid empty calendar. Queries run on a serial utility queue; RPC handlers do not wait for EventKit. Results crossing a day/timezone change are discarded and fetched again.

Entry points under `tools/dayring-cli/Sources/DayringCLI/Calendar/`:

| File | Responsibility |
| --- | --- |
| `CalendarSnapshot.swift` | Window, occurrence normalization, identity, JSON schema and capacity checks |
| `EventKitCalendarSource.swift` | Permission, source observation, date-range enumeration |
| `CalendarRPCService.swift` | Async preparation, immutable paging, relevance comparison and notification retry |
| `CalendarRunner.swift` | CLI command and BLE lifecycle integration |

### RPC

Methods 0–7 remain assigned to existing services; the CLI calendar methods are:

| ID | Method | Contract |
| --- | --- | --- |
| 8 | `calendar.begin` | Empty device request prepares a fresh snapshot. Error 3 means preparing: retry with backoff. Success is JSON `{snapshotId, byteLength, chunkBytes}`. |
| 9 | `calendar.changed` | Mac sends an empty request only after a relevant normalized change. Device acknowledges with an empty response and initiates a pull. |
| 10 | `calendar.read` | Request JSON `{snapshotId, offset}`; response is up to 10,240 raw bytes of the pinned JSON snapshot at that byte offset. |
| 11 | `calendar.status` | Empty request to ESP32; returns the read-only 12-byte synchronization/persistence/count status described below. |

Status bytes: version 1 at byte 0; sync state (0 waiting, 1 syncing, 2 synchronized, 3 failed) at byte 1; persistence (0 unavailable, 1 saved, 2 pending, 3 failed) at byte 2; failure (0 none, 1 transport, 2 invalid snapshot, 3 capacity, 4 time context) at byte 3; little-endian complete-event count at bytes 4–5 and upcoming count at bytes 6–7; current-coverage and usable-time flags at bytes 8 and 9; reserved zero bytes at 10–11. The CLI requests this status after serving a complete snapshot; serving bytes alone is not proof of device commit or persistence. This diagnostic contains no titles, locations, or event IDs.

One successful begin pins one immutable snapshot per session. A later successful begin replaces its token. Reads renew a ten-minute inactivity expiry. Disconnect/session reset invalidates tokens. Repeated reads at the same offset return identical bytes, regardless of source changes during transfer. Assemble exactly `byteLength` bytes before decoding UTF-8/JSON because chunks may split characters.

Malformed requests, unknown/expired tokens, and invalid offsets return error 2. Source/permission/validation/capacity failures return error 7 from begin, not an empty success. Preparation returns error 3 and retries do not repeatedly restart the query. RPC fragmentation handles each logical chunk independently; see [RPC](rpc.md).

Safety budgets reject the entire result: 10,000 enumerated non-cancelled candidates, 16 KiB UTF-8 per title/location, and 2 MiB encoded JSON. These are CLI engineering budgets, not display limits. The ESP32 budgets below are smaller; exceeding them rejects a snapshot rather than truncating it.

### Relevant-change notifications

EventKit store notifications do not identify individual changes. macOS re-queries the current today/tomorrow window, flattens and normalizes it, and compares membership and the six fields with its previous observed result. Ignore query order, generation timestamps, and unused metadata.

Edits to a completed event within the window are relevant and must notify the device. Edits only affecting dates after tomorrow or before today do not notify unless an occurrence moves into/out of the window or overlaps it. Merely reaching an event's end does not remove it from this service's snapshot and does not send a notification.

Source changes during enumeration set a dirty flag for another query. Day/timezone changes rebuild the comparison baseline; the device's own context-change pull replenishes its cache. Long main-loop pauses and wall-clock jumps trigger reconciliation. Queries are serialized, with one pending change flag rather than an unbounded debounce.

Failed source queries retry after 30 seconds or a source-change signal. Failed invalidation delivery retries after five seconds. An acknowledgement of an older notification cannot clear a newer change. Existing firmware returning unknown-method for method 9 disables further notifications for that session with a diagnostic; reconnect permits another attempt. The new ESP32 CalendarService implements method 9 and initiates the corresponding pull; older firmware remains compatible with the fallback.

CalendarService serializes pulls, retains one follow-up pull when notified during a transfer, and retries failed pulls while connected. Notification receipt is not installation acknowledgement. Periodic full polling is not required for the first version.

## ESP32 CalendarService

The boundary is explicit: **CLI/RPC and the persisted cache contain complete today/tomorrow data; only the local `upcoming` query has upcoming semantics.** Completed events remain in the snapshot, regardless of the current time or the caller's result limit.

ServiceManager owns CalendarService after TimeService, so RPC and cached time are available during start and remain available during reverse-order stop. Use `Shell::services().calendar()`. The service owns no application/controller pointers and performs no rendering.

Public API:

- `synchronize()` requests a pull on the next owning-loop update, without overlapping active work. Method 9 uses this same path.
- `upcoming(maxCount)` returns owned `UpcomingEvent` values, each containing the six-field event and a computed `isOngoing`. Zero returns no rows; omitted limit returns all available upcoming rows. Ordering is start instant, then instance ID, including all-day events in that order.
- `subscribeUpcoming(callback)` returns a move-only RAII subscription. Keep it alive to remain subscribed. Callbacks receive `UpcomingChange` with a revision and reason (`Snapshot`, `Time`, or `Restored`). Read the current list when subscribing/entering a page; subscriptions do not replay historical events.
- `snapshot()` returns a shared immutable complete snapshot, safe to retain across later replacements. `hasSnapshot()`, `hasCurrentCoverage()`, and `hasUsableTime()` distinguish available data from missing/expired/time-incompatible data.
- `syncState()`, `failure()`, and `persistenceState()` expose synchronization and file-save health independently. A failed operation may coexist with usable old data.

Positive-duration events are upcoming while `end > now`, including ongoing events where `start <= now < end`. Zero-duration events remain upcoming through their start second. All-day events end at the exclusive local-midnight boundary. No query removes items from the full snapshot or rewrites its file.

CalendarService reads TimeService's cached sample each loop; it does not read/write RTC hardware independently. It recomputes the upcoming selection when the time sample changes. Membership, the returned event fields, and `isOngoing` all participate in change detection, so crossing a start boundary can notify even when list membership is unchanged. Identical lists do not notify, and changes exclusively to past events do not emit an upcoming-list event. Callbacks run on the owning loop; subscription destruction and subscription changes during a callback are safe. Lifecycle start/stop must remain outside callbacks, following the existing service contract.

A restored local RTC can use the persisted offset only when the snapshot's start/end/generation offsets agree. A different current timezone or expired coverage makes the projection unavailable. When TimeService has timezone/offset knowledge, it remains authoritative. This does not add an offline timezone-rule database or fix TimeService's existing offline DST limitation.

### Pull and validation

On each RPC-ready session, immediately begin a full pull. Also request a pull on method 9, local date rollover, or timezone/offset change. An in-flight pull completes or fails independently of a latched follow-up request. Disconnect discards staging and cancels requests; committed data stays available. Stop unregisters method 9 and cancels requests before RPC shuts down.

RPC callbacks only copy bounded complete replies. Parsing and commit run from the service update. Begin/read use methods 8/10 and validate manifest fields, exact chunk sizes, total length, JSON schema, strict UTF-8, unique IDs, offset-bearing date-times, natural-day coverage, and current date/timezone compatibility. Stale-window, malformed, partial and oversized snapshots never replace committed data. A complete empty snapshot is valid and clears events.

Busy replies retry after one second. Preparation is bounded to two minutes and the whole transfer to twenty minutes, with existing 120-second per-request RPC deadlines. Other failures retry after 30 seconds; a new notification or reconnect can trigger an earlier attempt. No periodic full poll is needed when the source, date and timezone are unchanged.

Device safety budgets are 64 KiB encoded JSON, 256 events, 2 KiB UTF-8 per title/location, 128-byte IDs, and a 64-byte timezone name, with bounded structural tokens and nesting. These are resource limits, not daily truncation rules. The source CLI can represent larger datasets; a device rejects those as invalid/unsupported snapshots, preserves its prior snapshot, and reports failure. Capacity/heap behavior near these limits still needs physical measurement.

### FATFS persistence

The user chose FATFS instead of the earlier SPIFFS proposal. Keep the existing `storage` partition (`data,fat`, offset `0x410000`, size `0xBE0000`) and `board_build.filesystem = fatfs`. Mount through `FFat.begin(false, "/ffat", 4, "storage")`. Mount failure does not format or erase the partition; an unformatted/damaged filesystem requires deliberate provisioning/recovery outside this service. Native previews use an unavailable file backend rather than writing host files.

After complete validation, atomically publish the new immutable snapshot in RAM and emit any upcoming change, then save it. A file-save failure leaves the fresh in-memory data available, exposes `PersistenceState::Failed`, and retries after 30 seconds. A reboot before persistence succeeds can restore the last older valid file; its coverage and generation time remain explicit. Unchanged event content and coverage do not trigger another write solely because the generation timestamp changed.

`/calendar-a.cache` and `/calendar-b.cache` alternate as recovery slots. Each contains a 24-byte binary header (`DRCAL001`, little-endian generation u64, payload length u32, CRC-32 u32) followed by the exact received JSON bytes. CRC covers header metadata and payload, excluding its own four bytes. Write only the inactive slot, call `fsync`, close, and verify readback. Load validates both slots' length, checksum and schema, choosing the newest valid generation. Interrupted/corrupt writes fall back to the other valid slot; no FAT rename atomicity assumption is required. The scheme cannot guarantee recovery from filesystem-wide corruption, and actual power-cut behavior still requires hardware testing.

### Implementation map

| Responsibility | File under `src/platform/calendar/` |
| --- | --- |
| Model, bounded schema/UTF-8/date parsing, manifest codec | `Calendar.h`, `Calendar.cpp` |
| Cached TimeService to local/UTC context | `CalendarClock.h/.cpp` |
| Public lifecycle, local projection, subscription dispatch | `services/CalendarService.h/.cpp`, `UpcomingChanges.h/.cpp` |
| Single pull, paging, retry and atomic commit | `services/CalendarSync.cpp` |
| Two-file validation and recoverable replacement | `CalendarStorage.h/.cpp` |
| Existing FAT partition mount and flushed file I/O | `FatCalendarFiles.cpp` |

Firmware uses ESP-IDF cJSON 1.7.19. Native tests/previews use the matching unmodified MIT-licensed source under `third_party/cjson`, through `Json.cpp`. No global allocator hooks or third-party edits are introduced.

### Remaining scope

Lock-screen UI now renders up to three local upcoming events; Home integration, partition migration, and an iOS EventKit provider are outside the completed scope. iOS peers can implement the same source protocol; the existing provider is the macOS CLI. A new valid snapshot wholly replaces the previous one and never merges peer datasets. Ownership/cache deletion policy for permission revocation, unpairing and switching peers remains future product work.

## Verification

`make test-calendar` covers full-cache/upcoming separation, max counts, time and snapshot notifications, cancellation, busy/retry, malformed/short pages, multi-page pulls, session isolation, cache restore, failed writes and corrupt/interrupted file recovery. `make test-calendar-interop` sends a synthetic Swift-generated UTF-8 snapshot through the C++ pull state machine and restores its persisted data; no personal calendars or radios are used. ServiceManager lifecycle, native preview builds and the ESP32 firmware build include the new service.

`make test-cli-calendar` covers today/tomorrow boundaries, retained completed events, no time-of-day filtering, cancelled/out-of-window exclusions, zero-duration and cross-day membership, DST 47/49-hour windows, exact six-field JSON, unlimited-by-product counts, immutable multi-page reads, real RPCPeer round trips, retry, change races and session isolation. `make test-cli-administration` verifies that command parsing and existing administration remain intact. `make test-rpc-interop` covers existing Swift/C++ framing.

A user-requested real EventKit read after the correction verified today/tomorrow coverage and retention of completed events. It also exposed inclusive last-second all-day source endpoints, now covered by normalization and a regression test. No personal event details are stored in this document. Physical BLE exchange, FAT mount/provisioning, power-cut recovery and card rendering are not claimed by these host tests.

## Source notes

Apple documentation consulted on 2026-09-21:

- [Updating with notifications](https://developer.apple.com/documentation/eventkit/updating-with-notifications): refetch the relevant date range after store changes.
- [EKEventStoreChanged](https://developer.apple.com/documentation/foundation/nsnotification/name-swift.struct/ekeventstorechanged): notifications do not describe individual changes.
- [Retrieving events and reminders](https://developer.apple.com/documentation/eventkit/retrieving-events-and-reminders): use date-range queries; lookup by identifier is not a recurrence expansion strategy.
- [occurrenceDate](https://developer.apple.com/documentation/eventkit/ekevent/occurrencedate), [eventIdentifier](https://developer.apple.com/documentation/eventkit/ekevent/eventidentifier), [calendarItemIdentifier](https://developer.apple.com/documentation/eventkit/ekcalendaritem/calendaritemidentifier), and [calendarItemExternalIdentifier](https://developer.apple.com/documentation/eventkit/ekcalendaritem/calendaritemexternalidentifier): occurrence anchors and identity limitations.
- [Accessing the event store](https://developer.apple.com/documentation/eventkit/accessing-the-event-store): read-capable calendar authorization and macOS sandbox calendar entitlement requirements.

## Physical verification (2026-09-21)

The latest calendar lock-screen firmware, including RobotoS at 22 px, compact title-only all-day rows, and the two-stage touch policy (eight-second lighting and a separate five-second hint), was uploaded through the existing USB preparation hook and esptool verified its hash. A targeted BLE connection to the paired PaperMono completed secure pairing, RPC hello/ping and RTC synchronization (`2026-09-21 21:02:28`, Asia/Shanghai). The real EventKit snapshot was 1,480 bytes; device status reported synchronized, saved, six complete occurrences, five upcoming occurrences, valid current coverage and no synchronization error. FAT save includes flushed write and readback verification. The CLI remains connected for change notifications.

This verifies real BLE pull and FAT persistence/readback, not a physical power-cut recovery test. No formatting, pairing reset, or calendar-source mutation was performed. No personal event details are stored in this document.
