import EventKit
import Foundation

/// EventKit objects stay on this serial queue; only value records cross to the main queue.
final class EventKitCalendarSource {
    var onChange: (() -> Void)?
    private let _queue = DispatchQueue(label: "dev.dayring.calendar", qos: .utility)
    private let _store = EKEventStore()
    private var _observer: NSObjectProtocol?

    init() {
        _observer = NotificationCenter.default.addObserver(forName: .EKEventStoreChanged, object: _store,
                                                          queue: .main) { [weak self] _ in self?.onChange?() }
    }

    deinit {
        if let observer = _observer { NotificationCenter.default.removeObserver(observer) }
    }

    func authorize(_ completion: @escaping (Bool) -> Void) {
        let callback: EKEventStoreRequestAccessCompletionHandler = { granted, _ in
            DispatchQueue.main.async { completion(granted) }
        }
        _queue.async { [self] in
            if #available(macOS 14, *) { _store.requestFullAccessToEvents(completion: callback) }
            else { _store.requestAccess(to: .event, completion: callback) }
        }
    }

    func fetch(window: CalendarWindow, completion: @escaping (Result<[CalendarCandidate], Error>) -> Void) {
        _queue.async { [self] in
            let result = Result { () throws -> [CalendarCandidate] in
                guard Self._canRead() else { throw CalendarFailure.permission }
                let predicate = _store.predicateForEvents(withStart: window.start, end: window.end, calendars: nil)
                var candidates: [CalendarCandidate] = []
                var failure: Error?
                _store.enumerateEvents(matching: predicate) { event, stop in
                    if event.status == .canceled { return }
                    guard let start = event.startDate, let end = event.endDate,
                          let calendar = event.calendar, !event.calendarItemIdentifier.isEmpty,
                          end >= start else {
                        failure = CalendarFailure.invalidSource; stop.pointee = true; return
                    }
                    // EventKit expands recurrences; occurrenceDate preserves a detached instance's original anchor.
                    let anchor = event.occurrenceDate ?? Date(timeIntervalSince1970: 0)
                    candidates.append(CalendarCandidate(
                        instanceId: CalendarCandidate.identity(calendar: calendar.calendarIdentifier,
                            item: event.calendarItemIdentifier, occurrence: anchor),
                        title: event.title ?? "", location: event.location ?? "", start: start, end: end,
                        isAllDay: event.isAllDay, isCancelled: event.status == .canceled))
                    if candidates.count > 10_000 { failure = CalendarFailure.capacity; stop.pointee = true }
                }
                if let failure { throw failure }
                guard Self._canRead() else { throw CalendarFailure.permission }
                return candidates
            }
            DispatchQueue.main.async { completion(result) }
        }
    }

    private static func _canRead() -> Bool {
        let status = EKEventStore.authorizationStatus(for: .event)
        if #available(macOS 14, *) { return status == .fullAccess }
        return status == .authorized
    }
}
