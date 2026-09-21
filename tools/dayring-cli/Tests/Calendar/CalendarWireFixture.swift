import Foundation

@main
struct CalendarWireFixture {
    static func main() throws {
        let formatter = ISO8601DateFormatter()
        let now = formatter.date(from: "2026-09-21T12:00:00+08:00")!
        let window = CalendarWindow(now: now, timeZone: TimeZone(identifier: "Asia/Shanghai")!)
        var candidates = (0..<90).map { index in
            CalendarCandidate(instanceId: "fixture-\(index)", title: "会议 \"\(index)\" 😀", location: "Room\n二楼",
                start: now.addingTimeInterval(3600 + Double(index) * 60),
                end: now.addingTimeInterval(7200 + Double(index) * 60), isAllDay: false, isCancelled: false)
        }
        candidates.append(CalendarCandidate(instanceId: "past", title: "Completed", location: "",
            start: now.addingTimeInterval(-7200), end: now.addingTimeInterval(-3600), isAllDay: false, isCancelled: false))
        FileHandle.standardOutput.write(try CalendarSnapshot(window: window, now: now, candidates: candidates).encoded())
    }
}
