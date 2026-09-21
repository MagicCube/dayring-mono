import XCTest
@testable import DayringBLE

final class TimeZoneMonitorTests: XCTestCase {
    func testTravelDSTAndRetry() {
        var snapshot = TimeZoneMonitor.Snapshot(identifier: "Asia/Shanghai", offset: 28800)
        let monitor = TimeZoneMonitor(sample: { snapshot })
        var calls = 0
        var completion: ((Bool) -> Void)?
        let notify: (@escaping (Bool) -> Void) -> Void = { calls += 1; completion = $0 }
        monitor.poll(now: 0, notify: notify)
        XCTAssertEqual(calls, 0)
        snapshot = .init(identifier: "Europe/Paris", offset: 7200)
        monitor.poll(now: 10, notify: notify)
        XCTAssertEqual(calls, 1)
        monitor.poll(now: 20, notify: notify)
        XCTAssertEqual(calls, 1)
        completion?(false)
        monitor.poll(now: 21, notify: notify)
        XCTAssertEqual(calls, 2)
        completion?(true)
        monitor.poll(now: 31, notify: notify)
        XCTAssertEqual(calls, 2)
        snapshot = .init(identifier: "Europe/Paris", offset: 3600)
        monitor.poll(now: 41, notify: notify)
        XCTAssertEqual(calls, 3)
        completion?(true)
        // Moving between zones with the same current offset still changes the identifier.
        snapshot = .init(identifier: "Europe/Berlin", offset: 3600)
        monitor.poll(now: 51, notify: notify)
        XCTAssertEqual(calls, 4)
    }
}
