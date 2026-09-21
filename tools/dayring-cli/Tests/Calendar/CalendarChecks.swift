import DayringBLE
import Foundation

private func date(_ text: String) -> Date { ISO8601DateFormatter().date(from: text)! }
private func candidate(_ id: String, _ start: String, _ end: String, title: String = "Meeting",
                       allDay: Bool = false, cancelled: Bool = false) -> CalendarCandidate {
    CalendarCandidate(instanceId: id, title: title, location: "Room", start: date(start), end: date(end),
                      isAllDay: allDay, isCancelled: cancelled)
}

private final class Harness {
    var now = date("2026-09-30T14:00:00+08:00")
    var zone = TimeZone(identifier: "Asia/Shanghai")!
    var ticks: TimeInterval = 0
    var fetches: [(CalendarWindow, (Result<[CalendarCandidate], Error>) -> Void)] = []
    var notifications: [(Result<Data, RPCError>) -> Void] = []
    lazy var service = CalendarRPCService(fetch: { [unowned self] window, completion in
        fetches.append((window, completion))
    }, notify: { [unowned self] completion in notifications.append(completion) },
    now: { [unowned self] in now }, zone: { [unowned self] in zone }, ticks: { [unowned self] in ticks })

    func finish(_ values: [CalendarCandidate]) { fetches.removeFirst().1(.success(values)) }
    func fail() { fetches.removeFirst().1(.failure(CalendarFailure.permission)) }
    func busy(_ reply: Result<Data, RPCPeer.UInt8Error>, code: UInt8 = 3) {
        guard case .failure(let error) = reply else { preconditionFailure("expected error") }
        precondition(error.code == code)
    }
    func manifest() throws -> [String: Any] {
        try JSONSerialization.jsonObject(with: service.begin(Data()).get()) as! [String: Any]
    }
    func read(_ manifest: [String: Any], offset: Int = 0) -> Result<Data, RPCPeer.UInt8Error> {
        service.read(try! JSONSerialization.data(withJSONObject: ["snapshotId": manifest["snapshotId"]!, "offset": offset]))
    }
}

@main
struct CalendarChecks {
    static func main() throws {
        try modelChecks()
        try changeChecks()
        try pagingChecks()
        try recoveryChecks()
        try contextChecks()
        try rpcChecks()
        try capacityChecks()
        print("Calendar normalization, natural-day/DST boundaries, filtered notifications, immutable paging, retries and session checks passed")
    }

    static func modelChecks() throws {
        let now = date("2026-09-30T14:00:00+08:00")
        let window = CalendarWindow(now: now, timeZone: TimeZone(identifier: "Asia/Shanghai")!)
        precondition(window.start == date("2026-09-30T00:00:00+08:00"))
        precondition(window.end == date("2026-10-02T00:00:00+08:00"))
        var values = [candidate("past", "2026-09-30T12:00:00+08:00", "2026-09-30T14:00:00+08:00"),
                      candidate("ongoing", "2026-09-29T23:00:00+08:00", "2026-09-30T15:00:00+08:00"),
                      candidate("cancelled", "2026-10-01T10:00:00+08:00", "2026-10-01T11:00:00+08:00", cancelled: true),
                      candidate("outside", "2026-10-02T00:00:00+08:00", "2026-10-02T01:00:00+08:00"),
                      candidate("allDay", "2026-09-30T00:00:00+08:00", "2026-10-01T00:00:00+08:00", allDay: true),
                      candidate("point", "2026-09-30T14:00:00+08:00", "2026-09-30T14:00:00+08:00")]
        for index in 0..<8 {
            values.append(candidate("tomorrow\(index)", "2026-10-01T10:00:00+08:00", "2026-10-01T11:00:00+08:00"))
        }
        values += [candidate("before", "2026-09-29T22:00:00+08:00", "2026-09-30T00:00:00+08:00"),
                   candidate("after", "2026-10-02T00:00:00+08:00", "2026-10-02T01:00:00+08:00")]
        let snapshot = try CalendarSnapshot(window: window, now: now, candidates: values.reversed())
        precondition(snapshot.events.count == 12)
        precondition(snapshot.events.contains { $0.instanceId == "past" })
        let later = try CalendarSnapshot(window: window, now: now.addingTimeInterval(3600), candidates: values)
        precondition(later.events == snapshot.events)
        precondition(snapshot.events.first?.instanceId == "ongoing")
        let json = try JSONSerialization.jsonObject(with: snapshot.encoded()) as! [String: Any]
        let events = json["events"] as! [[String: Any]]
        precondition(Set(events[0].keys) == Set(["instanceId", "title", "location", "start", "end", "isAllDay"]))
        precondition(try! window.occurrences(values) == snapshot.events)
        do { _ = try window.occurrences([values[1], values[1]]); preconditionFailure("duplicate") }
        catch CalendarFailure.invalidSource {}
        let inclusive = candidate("holiday", "2026-09-30T00:00:00+08:00", "2026-09-30T23:59:59+08:00", allDay: true)
        let holiday = try window.occurrences([inclusive])[0]
        precondition(holiday.end == "2026-10-01T00:00:00+08:00")
        let newYork = TimeZone(identifier: "America/New_York")!
        let spring = CalendarWindow(now: date("2026-03-07T12:00:00-05:00"), timeZone: newYork)
        let fall = CalendarWindow(now: date("2026-10-31T12:00:00-04:00"), timeZone: newYork)
        precondition(spring.end.timeIntervalSince(spring.start) == 47 * 3600)
        precondition(fall.end.timeIntervalSince(fall.start) == 49 * 3600)
        let crossing = candidate("dst", "2026-11-01T01:30:00-04:00", "2026-11-01T01:30:00-05:00")
        let dst = try fall.occurrences([crossing])[0]
        precondition(dst.start.hasSuffix("-04:00") && dst.end.hasSuffix("-05:00"))
        let anchor = date("2026-09-30T10:00:00Z")
        let id = CalendarCandidate.identity(calendar: "a", item: "series", occurrence: anchor)
        precondition(id != CalendarCandidate.identity(calendar: "b", item: "series", occurrence: anchor))
        precondition(id != CalendarCandidate.identity(calendar: "a", item: "series", occurrence: anchor.addingTimeInterval(86400)))
    }

    static func changeChecks() throws {
        let h = Harness()
        let event = candidate("a", "2026-09-30T14:00:00+08:00", "2026-09-30T15:00:00+08:00")
        h.service.setConnected(true); h.finish([event])
        precondition(h.notifications.isEmpty)
        h.service.sourceChanged(); h.finish([event])
        precondition(h.notifications.isEmpty)
        let outside = candidate("out", "2026-10-03T10:00:00+08:00", "2026-10-03T11:00:00+08:00")
        h.service.sourceChanged(); h.finish([outside, event])
        precondition(h.notifications.isEmpty)
        let changed = candidate("a", "2026-09-30T14:00:00+08:00", "2026-09-30T15:00:00+08:00", title: "Changed")
        h.service.sourceChanged(); h.finish([changed])
        precondition(h.notifications.count == 1)
        h.notifications.removeFirst()(.failure(.timeout))
        h.ticks = 5; h.service.update()
        precondition(h.notifications.count == 1)
        h.service.sourceChanged(); h.finish([])
        h.notifications.removeFirst()(.success(Data()))
        h.ticks = 10; h.service.update()
        precondition(h.notifications.count == 1, "change while notifying must survive older ACK")
        h.notifications.removeFirst()(.success(Data()))
        h.ticks = 20; h.service.update()
        precondition(h.notifications.isEmpty)
        let elapsed = Harness()
        elapsed.service.setConnected(true); elapsed.finish([event])
        elapsed.now = date("2026-09-30T15:01:00+08:00")
        elapsed.service.sourceChanged(); elapsed.finish([event])
        precondition(elapsed.notifications.isEmpty, "elapsed time must retain completed events")
        elapsed.service.sourceChanged(); elapsed.finish([])
        precondition(elapsed.notifications.count == 1, "deleting a completed event must notify")
    }

    static func pagingChecks() throws {
        let h = Harness()
        let values = (0..<100).map { candidate("\($0)", "2026-10-01T10:00:00+08:00", "2026-10-01T11:00:00+08:00", title: String(repeating: "会议", count: 20)) }
        h.service.setConnected(true); h.finish(values)
        h.busy(h.service.begin(Data([1])), code: 2)
        h.busy(h.service.begin(Data())); h.busy(h.service.begin(Data()))
        precondition(h.fetches.count == 1, "retries must not create endless fresh queries")
        h.finish(values)
        let manifest = try h.manifest()
        precondition(manifest["byteLength"] as! Int > RPCPeer.maximumPayloadSize)
        var bytes = Data()
        bytes.append(try h.read(manifest).get())
        h.service.sourceChanged(); h.finish([])
        while bytes.count < manifest["byteLength"] as! Int {
            bytes.append(try h.read(manifest, offset: bytes.count).get())
        }
        let restored = try JSONDecoder().decode(CalendarSnapshot.self, from: bytes)
        precondition(restored.events.count == 100, "pages must stay immutable during changes")
        h.busy(h.read(manifest, offset: -1), code: 2)
        h.busy(h.read(manifest, offset: bytes.count), code: 2)
        h.ticks = 601
        h.busy(h.read(manifest), code: 2)
        h.service.setConnected(false)
        h.busy(h.read(manifest), code: 2)
    }

    static func recoveryChecks() throws {
        let h = Harness()
        h.service.setConnected(true); h.fail()
        h.busy(h.service.begin(Data()), code: 7)
        precondition(h.fetches.isEmpty)
        h.ticks = 30; h.service.update(); h.finish([])
        let manifest = try h.manifest()
        let empty = try JSONDecoder().decode(CalendarSnapshot.self, from: h.read(manifest).get())
        precondition(empty.events.isEmpty)
        h.service.sourceChanged()
        let obsolete = h.fetches.removeFirst().1
        h.service.setConnected(false); h.service.setConnected(true)
        obsolete(.success([]))
        h.busy(h.service.begin(Data()))
        precondition(h.fetches.count == 1)
        h.finish([])
        h.service.update()
        if !h.fetches.isEmpty { h.finish([]) }
        _ = try h.manifest()
        h.service.sourceChanged()
        h.service.sourceChanged()
        h.finish([])
        h.service.update()
        precondition(h.fetches.count == 1, "change during enumeration must be re-queried")
        h.finish([])
    }

    static func rpcChecks() throws {
        let h = Harness()
        let values = (0..<80).map { candidate("wire\($0)", "2026-10-01T10:00:00+08:00", "2026-10-01T11:00:00+08:00") }
        h.service.setConnected(true); h.finish(values)
        let mac = RPCPeer(now: { 0 }), device = RPCPeer(now: { 0 })
        var toMac: [Data] = [], toDevice: [Data] = []
        mac.send = { toDevice.append($0); return true }
        device.send = { toMac.append($0); return true }
        mac.maximumPacketSize = { 244 }; device.maximumPacketSize = { 244 }
        mac.register(method: CalendarRPCService.beginMethod, handler: h.service.begin)
        mac.register(method: CalendarRPCService.readMethod, handler: h.service.read)
        func pump() throws {
            for _ in 0..<10_000 {
                if toMac.isEmpty && toDevice.isEmpty { return }
                if !toMac.isEmpty { try mac.receive(toMac.removeFirst()) }
                if !toDevice.isEmpty { try device.receive(toDevice.removeFirst()) }
            }
            preconditionFailure("RPC did not drain")
        }
        var response: Result<Data, RPCError>?
        device.request(method: 8) { response = $0 }; try pump()
        guard case .failure(.remote(3)) = response else { preconditionFailure("missing busy reply") }
        h.finish(values)
        device.request(method: 8) { response = $0 }; try pump()
        let manifest = try JSONSerialization.jsonObject(with: response!.get()) as! [String: Any]
        var assembled = Data()
        while assembled.count < manifest["byteLength"] as! Int {
            let request = try JSONSerialization.data(withJSONObject: ["snapshotId": manifest["snapshotId"]!, "offset": assembled.count])
            response = nil
            device.request(method: 10, payload: request) { response = $0 }; try pump()
            assembled.append(try response!.get())
        }
        let decoded = try JSONDecoder().decode(CalendarSnapshot.self, from: assembled)
        precondition(decoded.events.count == 80)
    }

    static func capacityChecks() throws {
        let window = CalendarWindow(now: date("2026-09-30T14:00:00+08:00"), timeZone: TimeZone(identifier: "Asia/Shanghai")!)
        let long = candidate("long", "2026-10-01T10:00:00+08:00", "2026-10-01T11:00:00+08:00", title: String(repeating: "x", count: 16_385))
        do {
            _ = try CalendarSnapshot(window: window, now: window.start, candidates: [long]).encoded()
            preconditionFailure("oversized field accepted")
        } catch CalendarFailure.capacity {}
        let values = (0..<300).map { candidate("large\($0)", "2026-10-01T10:00:00+08:00", "2026-10-01T11:00:00+08:00", title: String(repeating: "x", count: 10_000)) }
        do {
            _ = try CalendarSnapshot(window: window, now: window.start, candidates: values).encoded()
            preconditionFailure("oversized snapshot accepted")
        } catch CalendarFailure.capacity {}
    }

    static func contextChecks() throws {
        let h = Harness()
        h.service.setConnected(true); h.finish([])
        h.now = date("2026-10-01T00:00:00+08:00"); h.service.update()
        precondition(h.fetches[0].0.end == date("2026-10-03T00:00:00+08:00"))
        h.finish([])
        precondition(h.notifications.isEmpty, "rollover is reconciled by device pull")
        h.service.sourceChanged()
        h.now = date("2026-10-02T00:00:00+08:00")
        h.finish([]); h.service.update()
        precondition(h.fetches.count == 1, "discard query crossing midnight")
        h.finish([])
        h.zone = TimeZone(identifier: "America/New_York")!; h.service.update()
        precondition(h.fetches.count == 1)
        h.finish([])
    }
}
