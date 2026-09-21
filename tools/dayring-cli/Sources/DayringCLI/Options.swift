import DayringBLE
import Foundation

enum Command: String { case devServer = "dev-server", resetPairing = "reset-pairing", reboot, calendar }

struct Options {
    let command: Command
    var macOnly = false
    var listOnly = false
    var verifyPairing = true
    var pairingTimeout: TimeInterval = 60
    var service = DeviceProfile.dayringServiceUUID
    var device: UUID?
    var timeout: TimeInterval = 20
    var connectTimeout: TimeInterval = 15

    init(_ arguments: [String]) throws {
        guard let first = arguments.first, let command = Command(rawValue: first) else {
            throw UsageError.invalid("Expected dev-server, calendar, reset-pairing or reboot command")
        }
        self.command = command
        var index = 1
        while index < arguments.count {
            let option = arguments[index]
            if command == .calendar && option != "--timeout" {
                throw UsageError.invalid("calendar accepts only --timeout SECONDS")
            }
            if option == "--mac-only" { macOnly = true; index += 1; continue }
            if option == "--connect-only" { verifyPairing = false; index += 1; continue }
            if option == "--list" { listOnly = true; index += 1; continue }
            guard index + 1 < arguments.count else { throw UsageError.invalid("Missing value for \(option)") }
            let value = arguments[index + 1]
            switch option {
            case "--service", "--device":
                guard let uuid = UUID(uuidString: value) else { throw UsageError.invalid("Expected a full UUID for \(option)") }
                if option == "--service" { service = uuid } else { device = uuid }
            case "--timeout", "--connect-timeout", "--pair-timeout":
                guard let seconds = Double(value), seconds.isFinite, seconds > 0 else {
                    throw UsageError.invalid("Expected positive seconds for \(option)")
                }
                if option == "--timeout" { timeout = seconds }
                else if option == "--pair-timeout" { pairingTimeout = seconds }
                else { connectTimeout = seconds }
            default: throw UsageError.invalid("Unknown option: \(option)")
            }
            index += 2
        }
        if (command == .resetPairing || command == .reboot) && (device == nil || listOnly || !verifyPairing || service != DeviceProfile.dayringServiceUUID) {
            throw UsageError.invalid("Administrative commands require --device UUID and the Dayring service; --list and --connect-only are not allowed")
        }
        if command == .calendar && (device != nil || listOnly || !verifyPairing || service != DeviceProfile.dayringServiceUUID) {
            throw UsageError.invalid("calendar accepts only --timeout SECONDS and does not connect to Bluetooth")
        }
        if macOnly && command != .resetPairing { throw UsageError.invalid("--mac-only is valid only for reset-pairing") }
    }
}

enum UsageError: Error { case invalid(String) }
let usage = """
Usage: dayring-cli
       dayring-cli dev-server [--list] [--service UUID] [--device UUID]
                        [--timeout SECONDS] [--connect-timeout SECONDS]
                        [--pair-timeout SECONDS] [--connect-only]

       dayring-cli calendar [--timeout SECONDS]
       dayring-cli reset-pairing --device UUID [--mac-only] [--timeout SECONDS]
       dayring-cli reboot --device UUID [--timeout SECONDS]

No arguments defaults to dev-server. Use --help or -h for this help.

reset-pairing  Clear ESP32 bonds over RPC, restart it, and forget this device on macOS.
--mac-only     Recover a partial reset: forget only this exact macOS device without connecting to ESP32.
reboot         Request an ESP32 software restart over RPC; retain pairing information.
Stop any running dev-server before using either administrative command.

calendar       Print the complete today/tomorrow JSON snapshot; request calendar permission, no Bluetooth.

dev-server     Find a recognized peripheral, connect, verify its bond over an encrypted read, and hold until Ctrl-C.
--connect-only  Verify service without requesting pairing (for other test peripherals).
--list   List nearby advertisements without connecting; exit after the scan timeout.
Default service: \(DeviceProfile.dayringServiceUUID)
Default scan/startup timeout: 20s; connection/service discovery timeout: 15s each; pairing: 60s.
Exit codes: 0 completed/accepted, 1 failure or partial reset, 2 invalid arguments.
"""
