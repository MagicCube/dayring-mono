import Foundation

/// Poll while the app is active. On iOS suspension, polling resumes on wake;
/// reconnect always requests a fresh sample independently of this monitor.
public final class TimeZoneMonitor {
    public struct Snapshot: Equatable {
        public let identifier: String
        public let offset: Int
        public init(identifier: String, offset: Int) { self.identifier = identifier; self.offset = offset }
        public static func current() -> Snapshot {
            let zone = TimeZone.autoupdatingCurrent
            return Snapshot(identifier: zone.identifier, offset: zone.secondsFromGMT(for: Date()))
        }
    }
    private let _sample: () -> Snapshot
    private var _acknowledged: Snapshot
    private var _pending = false
    private var _nextCheck: TimeInterval = 0
    public init(sample: @escaping () -> Snapshot = Snapshot.current) {
        _sample = sample
        _acknowledged = sample()
    }
    public func poll(now: TimeInterval, notify: (@escaping (Bool) -> Void) -> Void) {
        guard !_pending, now >= _nextCheck else { return }
        _nextCheck = now + 10
        let current = _sample()
        guard current != _acknowledged else { return }
        _pending = true
        notify { [weak self] success in
            guard let self else { return }
            self._pending = false
            if success { self._acknowledged = current }
        }
    }
}
