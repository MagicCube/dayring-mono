import DayringBLE
import Foundation

private final class Store: PairingStore {
    var isReady = true
    var paired = true
    var supported = true
    var removes = 0
    var prepared: UUID?
    var removeImmediately = true
    func prepare(identifier: UUID) throws {
        if !supported { throw RPCError.unavailable }
        prepared = identifier
    }
    func isPaired() throws -> Bool { paired }
    func remove() throws { removes += 1; if removeImmediately { paired = false } }
}

@main
struct AdminChecks {
    static let device = UUID(uuidString: "11111111-2222-3333-4444-555555555555")!

    static func main() throws {
        try optionsChecks()
        try resetChecks()
        try failureChecks()
        try localAndRebootChecks()
        print("CLI administrative targeting, preflight, two-sided reset, partial failure, local recovery and reboot checks passed")
    }

    static func options(_ command: String, _ extra: [String] = []) throws -> Options {
        try Options([command, "--device", device.uuidString] + extra)
    }

    static func optionsChecks() throws {
        precondition(try! options("reset-pairing").command == .resetPairing)
        precondition(try! options("reboot").command == .reboot)
        precondition(try! options("reset-pairing", ["--mac-only"]).macOnly)
        precondition(try! Options(["dev-server", "--list"]).listOnly)
        precondition(try! Options(["calendar", "--timeout", "60"]).command == .calendar)
        for arguments in [["calendar", "--list"], ["calendar", "--connect-timeout", "10"],
                          ["calendar", "--device", device.uuidString], ["reset-pairing"], ["reboot"], ["dev-server", "--mac-only"],
                          ["reboot", "--device", device.uuidString, "--mac-only"],
                          ["reset-pairing", "--device", device.uuidString, "--connect-only"],
                          ["reset-pairing", "--device", device.uuidString, "--list"],
                          ["reboot", "--device", "bad"], ["reboot", "--device", device.uuidString, "--timeout", "nan"]] {
            do { _ = try Options(arguments); preconditionFailure("invalid options accepted: \(arguments)") }
            catch UsageError.invalid {} catch { throw error }
        }
    }

    static func resetChecks() throws {
        let store = Store()
        var now: TimeInterval = 0
        var calls: [Data] = [], stopped = 0, result: Int32?
        let runner = AdministrationRunner(options: try options("reset-pairing"), store: store,
            request: { method, payload, timeout, completion in
                precondition(method == 6 && timeout == 5)
                precondition(store.prepared == device)
                calls.append(payload)
                completion(.success(payload.isEmpty ? Data([0]) : Data([1])))
            }, stopLink: { stopped += 1 }, now: { now }, finish: { result = $0; precondition(!$1.isEmpty) })
        runner.rpcReady(identifier: device)
        precondition(calls == [Data()] && store.removes == 0 && result == nil)
        now = 0.1
        runner.update()
        precondition(calls == [Data(), Data([1])] && store.removes == 1 && stopped == 1)
        runner.update()
        precondition(result == 0 && stopped == 2)
        runner.rpcReady(identifier: device)
        runner.update()
        precondition(calls.count == 2 && store.removes == 1)
    }

    static func failureChecks() throws {
        let store = Store()
        var calls = 0, code: Int32?, message = ""
        store.supported = false
        let preflight = AdministrationRunner(options: try options("reset-pairing"), store: store,
            request: { _, _, _, _ in calls += 1 }, stopLink: {}, finish: { code = $0; message = $1 })
        preflight.rpcReady(identifier: device)
        precondition(code == 1 && calls == 0 && store.removes == 0)
        store.supported = true
        let failure = AdministrationRunner(options: try options("reset-pairing"), store: store,
            request: { _, _, _, completed in completed(.failure(.disconnected)) }, stopLink: {},
            finish: { code = $0; message = $1 })
        failure.rpcReady(identifier: device)
        precondition(code == 1 && store.removes == 0 && message.contains("--mac-only"))
        store.removeImmediately = false
        var now: TimeInterval = 0
        let partial = AdministrationRunner(options: try options("reset-pairing"), store: store,
            request: { _, _, _, completed in completed(.success(Data([1]))) }, stopLink: {},
            now: { now }, finish: { code = $0; message = $1 })
        partial.rpcReady(identifier: device)
        now = 21
        partial.update()
        precondition(code == 1 && store.removes == 1 && message.contains("ESP32 is cleared"))
        precondition(message.contains("System Settings"))
    }

    static func localAndRebootChecks() throws {
        let store = Store()
        var code: Int32?, calls = 0
        let local = AdministrationRunner(options: try options("reset-pairing", ["--mac-only"]), store: store,
            request: { _, _, _, _ in calls += 1 }, stopLink: {}, finish: { code = $0; _ = $1 })
        local.update(); local.update()
        precondition(code == 0 && store.removes == 1 && calls == 0 && store.prepared == device)
        code = nil
        let reboot = AdministrationRunner(options: try options("reboot"), store: nil,
            request: { method, payload, _, completed in
                precondition(method == 7 && payload.isEmpty)
                calls += 1
                completed(.success(Data()))
            }, stopLink: {}, finish: { code = $0; precondition($1.contains("accepted")) })
        reboot.rpcReady(identifier: device)
        precondition(code == 0 && calls == 1)
        var mismatchCalls = 0
        let mismatch = AdministrationRunner(options: try options("reboot"), store: nil,
            request: { _, _, _, _ in mismatchCalls += 1 }, stopLink: {}, finish: { code = $0; _ = $1 })
        mismatch.rpcReady(identifier: UUID())
        precondition(code == 1 && mismatchCalls == 0)
    }
}
