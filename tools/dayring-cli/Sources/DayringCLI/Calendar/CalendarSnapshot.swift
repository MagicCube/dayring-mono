import CryptoKit
import Foundation

struct CalendarOccurrence: Codable, Equatable {
    let instanceId: String
    let title: String
    let location: String
    let start: String
    let end: String
    let isAllDay: Bool
}

struct CalendarCandidate {
    let instanceId: String
    let title: String
    let location: String
    let start: Date
    let end: Date
    let isAllDay: Bool
    let isCancelled: Bool

    func normalized(in timeZone: TimeZone) -> CalendarCandidate {
        guard isAllDay, end >= start else { return self }
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = timeZone
        let endDay = calendar.startOfDay(for: end)
        let exclusiveEnd = end == endDay ? endDay : calendar.date(byAdding: .day, value: 1, to: endDay)!
        return CalendarCandidate(instanceId: instanceId, title: title, location: location,
            start: calendar.startOfDay(for: start), end: exclusiveEnd, isAllDay: true, isCancelled: isCancelled)
    }

    static func identity(calendar: String, item: String, occurrence: Date) -> String {
        let components = [calendar, item, String(occurrence.timeIntervalSince1970)]
        let key = components.map { "\($0.utf8.count):\($0)" }.joined()
        return SHA256.hash(data: Data(key.utf8)).map { String(format: "%02x", $0) }.joined()
    }
}

struct CalendarWindow: Equatable {
    let start: Date
    let end: Date
    let timeZone: TimeZone

    init(now: Date, timeZone: TimeZone) {
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = timeZone
        let today = calendar.startOfDay(for: now)
        start = today
        end = calendar.date(byAdding: .day, value: 2, to: today)!
        self.timeZone = timeZone
    }

    func occurrences(_ candidates: [CalendarCandidate]) throws -> [CalendarOccurrence] {
        let formatter = ISO8601DateFormatter()
        formatter.timeZone = timeZone
        formatter.formatOptions = [.withInternetDateTime]
        let normalized = candidates.map { $0.normalized(in: timeZone) }
        let eligible = normalized.filter {
            !$0.isCancelled && $0.start < end && $0.end >= $0.start &&
                ($0.end > $0.start ? $0.end > start : $0.start >= start)
        }.sorted { $0.start == $1.start ? $0.instanceId < $1.instanceId : $0.start < $1.start }
        var seen = Set<String>()
        return try eligible.map { event in
            guard seen.insert(event.instanceId).inserted else { throw CalendarFailure.invalidSource }
            return CalendarOccurrence(instanceId: event.instanceId, title: event.title, location: event.location,
                start: formatter.string(from: event.start), end: formatter.string(from: event.end), isAllDay: event.isAllDay)
        }
    }
}

struct CalendarSnapshot: Codable {
    let schemaVersion: Int
    let generatedAt: String
    let timeZone: String
    let windowStart: String
    let windowEndExclusive: String
    let events: [CalendarOccurrence]

    init(window: CalendarWindow, now: Date, candidates: [CalendarCandidate]) throws {
        let formatter = ISO8601DateFormatter()
        formatter.timeZone = window.timeZone
        schemaVersion = 1
        generatedAt = formatter.string(from: now)
        timeZone = window.timeZone.identifier
        windowStart = formatter.string(from: window.start)
        windowEndExclusive = formatter.string(from: window.end)
        events = try window.occurrences(candidates)
    }

    func encoded() throws -> Data {
        guard events.count <= 10_000,
              events.allSatisfy({ $0.title.utf8.count <= 16_384 && $0.location.utf8.count <= 16_384 }) else {
            throw CalendarFailure.capacity
        }
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        let data = try encoder.encode(self)
        guard data.count <= 2 * 1024 * 1024 else { throw CalendarFailure.capacity }
        return data
    }
}

enum CalendarFailure: Error, CustomStringConvertible {
    case permission, invalidSource, capacity
    var description: String {
        switch self {
        case .permission: return "Calendar read access is unavailable. Grant calendar access in System Settings and retry."
        case .invalidSource: return "Calendar source contains invalid or duplicate occurrence identities."
        case .capacity: return "Calendar snapshot exceeds the 2 MiB safety budget; no partial result was returned."
        }
    }
}
