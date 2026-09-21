import Foundation

/// Retry a fresh BLE connection, never replay an application request.
struct ConnectionRecovery {
    private(set) var attempts = 0
    mutating func nextDelay() -> TimeInterval? {
        guard attempts < 2 else { return nil }
        attempts += 1
        return TimeInterval(attempts)
    }
    mutating func connected() { attempts = 0 }
}
