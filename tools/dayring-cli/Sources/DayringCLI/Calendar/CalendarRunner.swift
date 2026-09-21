import DayringBLE
import Foundation

final class CalendarRunner {
    private let _source = EventKitCalendarSource()
    private let _central: BLECentral
    private var _authorizationFinished = false
    private var _connected = false
    private var _verifyAt: TimeInterval?
    private lazy var _service = CalendarRPCService(fetch: { [weak self] window, completion in
        guard let self, self._authorizationFinished else { completion(.failure(CalendarFailure.permission)); return }
        self._source.fetch(window: window, completion: completion)
    }, notify: { [weak self] completion in
        guard let self else { completion(.failure(.unavailable)); return }
        self._central.requestRPC(method: CalendarRPCService.changedMethod, timeout: 5, completion: completion)
    })

    init(central: BLECentral) {
        _central = central
        _service.onStatus = { fputs("\($0)\n", stderr) }
        _service.onSnapshotServed = { [weak self] in
            self?._verifyAt = ProcessInfo.processInfo.systemUptime + 1
        }
        _source.onChange = { [weak self] in self?._service.sourceChanged() }
        central.registerRPCHandler(method: CalendarRPCService.beginMethod) { [weak self] payload in
            self?._service.begin(payload) ?? .failure(.init(7))
        }
        central.registerRPCHandler(method: CalendarRPCService.readMethod) { [weak self] payload in
            self?._service.read(payload) ?? .failure(.init(7))
        }
        _source.authorize { [weak self] granted in
            guard let self else { return }
            self._authorizationFinished = true
            if !granted { fputs("\(CalendarFailure.permission)\n", stderr) }
            self._service.sourceChanged()
        }
    }

    func ready() {
        _connected = true
        _verifyAt = nil
        _service.setConnected(true)
    }

    func update() {
        if _connected && !_central.isRPCReady {
            _connected = false
            _verifyAt = nil
            _service.setConnected(false)
        }
        _service.update()
        if _connected, let verifyAt = _verifyAt, ProcessInfo.processInfo.systemUptime >= verifyAt {
            _verifyAt = nil
            _central.requestRPC(method: 11, timeout: 5) { result in
                guard case .success(let data) = result, data.count == 12, data[0] == 1 else {
                    fputs("Calendar device status unavailable: \(result)\n", stderr)
                    return
                }
                let sync = ["waiting", "syncing", "synchronized", "failed"]
                let storage = ["unavailable", "saved", "pending", "failed"]
                let syncState = Int(data[1]) < sync.count ? sync[Int(data[1])] : "unknown"
                let saveState = Int(data[2]) < storage.count ? storage[Int(data[2])] : "unknown"
                let count = Int(data[4]) | Int(data[5]) << 8
                let upcoming = Int(data[6]) | Int(data[7]) << 8
                print("Calendar device: sync=\(syncState), storage=\(saveState), events=\(count), upcoming=\(upcoming), coverage=\(data[8]), error=\(data[3])")
            }
        }
    }
}

func runCalendarCommand(timeout: TimeInterval) -> Int32 {
    let source = EventKitCalendarSource()
    var result: Result<Data, Error>?
    source.authorize { granted in
        guard granted else { result = .failure(CalendarFailure.permission); return }
        let now = Date()
        let zone = TimeZone(identifier: TimeZone.autoupdatingCurrent.identifier) ?? .current
        let window = CalendarWindow(now: now, timeZone: zone)
        source.fetch(window: window) { fetched in
            result = Result { try CalendarSnapshot(window: window, now: now, candidates: fetched.get()).encoded() }
        }
    }
    let deadline = ProcessInfo.processInfo.systemUptime + timeout
    while result == nil && ProcessInfo.processInfo.systemUptime < deadline {
        RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.05))
    }
    guard let result else { fputs("Calendar access/query timed out; retry after granting permission.\n", stderr); return 1 }
    switch result {
    case .success(let data):
        FileHandle.standardOutput.write(data)
        FileHandle.standardOutput.write(Data([10]))
        return 0
    case .failure(let error): fputs("\(error)\n", stderr); return 1
    }
}
