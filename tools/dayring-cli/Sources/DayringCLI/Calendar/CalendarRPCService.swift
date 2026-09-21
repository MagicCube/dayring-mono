import DayringBLE
import Foundation

/// Main-queue state machine. Fetch is asynchronous; RPC handlers never wait on EventKit.
final class CalendarRPCService {
    static let beginMethod: UInt16 = 8
    static let changedMethod: UInt16 = 9
    static let readMethod: UInt16 = 10
    typealias Fetch = (CalendarWindow, @escaping (Result<[CalendarCandidate], Error>) -> Void) -> Void
    typealias Notify = (@escaping (Result<Data, RPCError>) -> Void) -> Void
    var onStatus: ((String) -> Void)?
    var onSnapshotServed: (() -> Void)?
    private let _fetch: Fetch
    private let _notify: Notify
    private let _now: () -> Date
    private let _zone: () -> TimeZone
    private let _ticks: () -> TimeInterval
    private var _lastClock: (date: Date, ticks: TimeInterval)?
    private var _connected = false
    private var _epoch = 0
    private var _fetching = false
    private var _dirty = true
    private var _candidates: [CalendarCandidate]?
    private var _window: CalendarWindow?
    private var _failed = false
    private var _retryAt: TimeInterval = 0
    private var _revision = 0
    private var _acknowledged = 0
    private var _notifying = false
    private var _notifyAt: TimeInterval = 0
    private var _unsupported = false
    private var _preparing = false
    private var _prepared: Data?
    private var _transfer: Transfer?
    private struct Transfer { let id: String; let data: Data; let expires: TimeInterval }
    private struct Manifest: Encodable { let snapshotId: String; let byteLength: Int; let chunkBytes: Int }
    private struct Read: Decodable { let snapshotId: String; let offset: Int }

    init(fetch: @escaping Fetch, notify: @escaping Notify,
         now: @escaping () -> Date = Date.init,
         zone: @escaping () -> TimeZone = { TimeZone(identifier: TimeZone.autoupdatingCurrent.identifier) ?? .current },
         ticks: @escaping () -> TimeInterval = { ProcessInfo.processInfo.systemUptime }) {
        _fetch = fetch; _notify = notify; _now = now; _zone = zone; _ticks = ticks
    }

    func setConnected(_ connected: Bool) {
        _lastClock = nil
        _epoch += 1; _connected = connected; _fetching = false; _dirty = true
        _candidates = nil; _window = nil; _failed = false; _retryAt = 0
        _revision = 0; _acknowledged = 0; _notifying = false; _notifyAt = 0; _unsupported = false
        _preparing = false; _prepared = nil; _transfer = nil
        if connected { update() }
    }

    func sourceChanged() {
        _dirty = true
        _prepared = nil
        _retryAt = 0
        update()
    }

    func update() {
        guard _connected else { return }
        let now = _now(), ticks = _ticks()
        if let last = _lastClock, ticks - last.ticks > 30 ||
            abs(now.timeIntervalSince(last.date) - (ticks - last.ticks)) > 2 {
            _dirty = true; _prepared = nil
        }
        _lastClock = (now, ticks)
        let current = CalendarWindow(now: now, timeZone: _zone())
        if current != _window { _dirty = true }
        if _dirty && !_fetching && _ticks() >= _retryAt { _refresh(current) }
        if let transfer = _transfer, _ticks() >= transfer.expires { _transfer = nil }
        _sendInvalidation()
    }

    func begin(_ payload: Data) -> Result<Data, RPCPeer.UInt8Error> {
        guard payload.isEmpty else { return .failure(.init(2)) }
        guard _connected else { return .failure(.init(3)) }
        if let data = _prepared, !_dirty, !_fetching,
           _window == CalendarWindow(now: _now(), timeZone: _zone()) {
            _prepared = nil; _preparing = false
            let transfer = Transfer(id: UUID().uuidString, data: data, expires: _ticks() + 600)
            _transfer = transfer
            onStatus?("Calendar snapshot prepared: \(data.count) bytes")
            let manifest = Manifest(snapshotId: transfer.id, byteLength: data.count, chunkBytes: RPCPeer.maximumPayloadSize)
            return .success(try! JSONEncoder().encode(manifest))
        }
        if !_preparing { _preparing = true; _dirty = true }
        update()
        return .failure(.init(_failed ? 7 : 3))
    }

    func read(_ payload: Data) -> Result<Data, RPCPeer.UInt8Error> {
        guard _connected, let request = try? JSONDecoder().decode(Read.self, from: payload),
              let transfer = _transfer, transfer.id == request.snapshotId, _ticks() < transfer.expires,
              request.offset >= 0, request.offset < transfer.data.count else { return .failure(.init(2)) }
        _transfer = Transfer(id: transfer.id, data: transfer.data, expires: _ticks() + 600)
        let end = min(transfer.data.count, request.offset + RPCPeer.maximumPayloadSize)
        if end == transfer.data.count { onSnapshotServed?() }
        return .success(transfer.data.subdata(in: request.offset..<end))
    }

    private func _refresh(_ window: CalendarWindow) {
        _fetching = true; _dirty = false
        let epoch = _epoch
        _fetch(window) { [weak self] result in
            guard let self, self._connected, self._epoch == epoch else { return }
            self._fetching = false
            do {
                let candidates = try result.get()
                let now = self._now()
                guard window == CalendarWindow(now: now, timeZone: self._zone()) else {
                    self._dirty = true; return
                }
                let snapshot = try CalendarSnapshot(window: window, now: now, candidates: candidates)
                let data = try snapshot.encoded()
                if let previous = self._candidates, self._window == window,
                   try window.occurrences(previous) != snapshot.events { self._revision += 1 }
                self._candidates = candidates; self._window = window; self._failed = false
                if self._preparing && !self._dirty { self._prepared = data }
                self._retryAt = 0
            } catch {
                self._failed = true; self._dirty = true; self._prepared = nil
                self._retryAt = self._ticks() + 30
                self.onStatus?("Calendar refresh failed: \(error)")
            }
            self._sendInvalidation()
        }
    }

    private func _sendInvalidation() {
        guard _connected, !_unsupported, !_notifying, _revision != _acknowledged,
              _ticks() >= _notifyAt else { return }
        let revision = _revision, epoch = _epoch
        _notifying = true
        _notify { [weak self] result in
            guard let self, self._connected, self._epoch == epoch else { return }
            self._notifying = false
            if case .success(let data) = result, data.isEmpty { self._acknowledged = revision }
            else if case .failure(.remote(1)) = result {
                self._unsupported = true
                self.onStatus?("Calendar change notifications unsupported by this firmware; available again next session.")
            }
            self._notifyAt = self._ticks() + 5
        }
    }
}
