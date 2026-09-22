# Weather reports

## Ownership and data flow

ESP32 `platform::weather::WeatherService` actively requests weather from the connected CLI. ServiceManager starts it after CalendarService and stops it before RPC. `ServiceManager::weather()` exposes it independently of foreground applications. LockPageController reads its report and revision for the lock-screen weather card.

On each ready RPC session, the device requests method 12 immediately. After success, the next request is one hour later; after a failed submission, remote error, timeout or invalid report, retry is 30 seconds later while connected. The RPC deadline is 120 seconds. Only one weather request is outstanding. Unsigned monotonic elapsed arithmetic handles tick rollover. Disconnect cancels outstanding work; reconnect starts a fresh pull. Neither peer replays an old request.

Each request calls `https://wttr.in/?format=j1` without a city path, allowing wttr.in to locate the CLI's public IP. No Core Location lookup or location permission is used. A VPN/proxy can change the resolved city.

An ephemeral URLSession uses a 30-second request timeout and 45-second resource timeout. HTTP must succeed and the response must be valid. `current_condition[0].weatherCode` supplies the current WWO condition; `weather[0].mintempC`/`maxtempC` supply the API's first (today) forecast range. City comes from `nearest_area[0].areaName[0].value`. There is no CLI background refresh or weather cache, and no URL cache or disk persistence. Logs identify the HTTP stage and include the error domain/code, distinguishing URLSession timeouts from RPC failures.

## RPC method 12: weather.get

An empty request is sent from ESP32 to the CLI. The asynchronous handler returns one complete report on the same RPC request; no business polling method is needed. Integers are little endian.

| Bytes | Value |
| --- | --- |
| 0 | Version, currently 1 |
| 1–2 | Nonzero WWO weather condition code, unsigned 16-bit |
| 3–4 | Minimum temperature in whole degrees Celsius, signed 16-bit |
| 5–6 | Maximum temperature in whole degrees Celsius, signed 16-bit |
| 7 | City UTF-8 byte length, 1–128 |
| 8 onward | Exact city bytes, no terminator |

Temperatures must satisfy -100 ≤ minimum ≤ maximum ≤ 100. Cities must be nonempty valid UTF-8 without control characters. The complete response is 9–136 bytes and uses existing D2 fragmentation when longer than 12 bytes. Nonempty requests return invalid payload (2); concurrent weather work returns busy (3); network/decoding failures return internal failure (7). No fabricated default weather is sent.

The WWO code identifies the current condition, not a synthetic all-day summary, and is distinct from WMO codes. Reference: [WWO condition codes](https://www.worldweatheronline.com/weather-api/api/docs/weather-icons.aspx).

## Local query and daily persistence

```cpp
const auto& weather = services.weather();
if (const auto& report = weather.report()) {
    // report->weatherCode, report->minTempC, report->maxTempC, report->city
}
```

`report()` is `const std::optional<WeatherReport>&`. Startup loads the latest validated FATFS cache and compares its YYYYMMDD date with the cached TimeService local RTC date. An exact match exposes the report immediately, before BLE is ready. Missing, corrupt, expired or not-yet-date-verifiable cache data yields `std::nullopt`. Restoring a cache does not postpone the immediate connected pull or claim successful network synchronization. Callers must not retain the borrowed report across service updates.

Each successful pull stores the report with the device date sampled when the request began. A date change, including midnight or clock correction, cancels pending weather work, invalidates mismatched visible data and requests a new report when connected. The UI revision also changes on restoration and expiry. When the date is unavailable, production waits for a valid RTC date before pulling or restoring data. The date follows the device's local RTC convention; it is not a forecast timezone inferred from IP geolocation.

`WeatherStorage` owns persistence separately from service scheduling. `/weather-a.cache` and `/weather-b.cache` use alternating generation-numbered records with a versioned magic, date, bounded weather payload and CRC32 over header/data. Saves flush and close the inactive slot and verify readback before replacing the active generation. Interrupted or corrupted latest writes fall back to the previous valid record, which must still match today's date. The shared FATFS mount never formats on failure. Unchanged same-day reports do not rewrite flash; failed writes retain the RAM report and retry after 30 seconds. No NVS writes are used. `persistenceState()` reports Unavailable, Pending, Saved or Failed independently of network `syncState()`.

`reportRevision()` increments when the visible optional report changes; the lock controller checks it each update, independently of the minute clock, to invalidate the frame promptly. The card shows `--` before the first report, then a Sans M WWO condition label and `min - max°`. Its 128-pixel row starts 16 pixels after the reserved charging-label bounds, with a horizontally centered Stack group containing a 55-pixel dot icon slot, a 64-pixel B/W dithered divider, and a two-line Sans M weather Stack. Both gaps are 16 pixels; equal outer flex slots center the measured group, and vertical flex slots center each member.

All 48 WWO conditions map to six icon categories: clear, partly cloudy, cloud/fog, rain, lightning, and snow/ice. Merging affects icons only: detailed weather labels still distinguish precipitation intensity, fog, sleet and freezing conditions. Unknown codes use an empty icon and `--`. Weather assets use 11x11 matrices; Lock draws touching 5-pixel circles (55x55), 2.4 times smaller than the 12-pixel gallery dots (132x132). The reusable `DotMatrixView` reads grid size from its matrix and dot diameter from Props. The [generator](../tools/generate-dot-icons/README.md) supports `--size` for other grids, including 7x7 and 9x9.

`syncState()` exposes Waiting, Pending, Synchronized or Failed. A retained same-day report can be stale after a missed refresh; consumers must not interpret a retained value as proof of freshness. Querying does not trigger network or hardware access. Service methods and reads belong on the owning application loop.

## Verification

- `make test-weather`: decoder validation, initial empty value, success, hourly cadence, 30-second retry, failed-refresh retention, timeout, tick rollover, reconnect, same-day restart restoration, old/unknown dates, midnight cancellation, unchanged-write suppression, write retry and interrupted/corrupt slot recovery.
- `make test-cli-weather`: on-demand-only fetches, payload validation, concurrent request rejection, failure/disconnect handling, stale callback isolation, IP-based endpoint and city decoding, async RPC bounds/deadlines, and Swift-to-C++ wire fixture.
- `make test-service-manager test-rpc-interop`: integrated lifecycle and existing bidirectional RPC regression checks.

These tests use synthetic reports and do not request real location permission, access weather over HTTPS, or change hardware. The complete BLE/weather path requires a separate physical run.
