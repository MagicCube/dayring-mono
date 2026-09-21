import Foundation

public enum ClockSample {
    public static func encode(date: Date = Date(), timeZone: TimeZone = .current) -> Data? {
        let seconds = date.timeIntervalSince1970
        guard seconds >= 946684800, seconds <= 4102444799 else { return nil }
        let epoch = UInt64(seconds)
        let offset = UInt32(bitPattern: Int32(timeZone.secondsFromGMT(for: date)))
        return Data((0..<8).map { UInt8(truncatingIfNeeded: epoch >> ($0 * 8)) }
                    + (0..<4).map { UInt8(truncatingIfNeeded: offset >> ($0 * 8)) })
    }
}
