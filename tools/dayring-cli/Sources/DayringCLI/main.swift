import Darwin
import DayringBLE
import Foundation

struct Options {
    var listOnly = false
    var verifyPairing = true
    var pairingTimeout: TimeInterval = 60
    var service = DeviceProfile.dayringServiceUUID
    var device: UUID?
    var timeout: TimeInterval = 20
    var connectTimeout: TimeInterval = 15

    init(_ arguments: [String]) throws {
        guard arguments.first == "dev-server" else { throw UsageError.invalid("Expected dev-server command") }
        var index = 1
        while index < arguments.count {
            let option = arguments[index]
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
    }
}

enum UsageError: Error { case invalid(String) }
let usage = """
Usage: dayring-cli
       dayring-cli dev-server [--list] [--service UUID] [--device UUID]
                        [--timeout SECONDS] [--connect-timeout SECONDS]
                        [--pair-timeout SECONDS] [--connect-only]

No arguments defaults to dev-server. Use --help or -h for this help.

dev-server     Find a recognized peripheral, connect, verify its bond over an encrypted read, and hold until Ctrl-C.
--connect-only  Verify service without requesting pairing (for other test peripherals).
--list   List nearby advertisements without connecting; exit after the scan timeout.
Default service: \(DeviceProfile.dayringServiceUUID)
Default scan/startup timeout: 20s; connection/service discovery timeout: 15s each; pairing: 60s.
Exit codes: 0 stopped/list complete, 1 BLE failure/disconnection, 2 invalid arguments.
"""
let arguments = Array(CommandLine.arguments.dropFirst())
if arguments == ["--help"] || arguments == ["-h"] {
    print(usage)
    exit(0)
}
let options: Options
do { options = try Options(arguments.isEmpty ? ["dev-server"] : arguments) }
catch {
    fputs("\(error)\n\(usage)\n", stderr)
    exit(2)
}
setbuf(stdout, nil)
let central = BLECentral(profile: DeviceProfile(serviceUUID: options.service, peripheralID: options.device))
var finished = false
var exitCode: Int32 = 0
central.onEvent = { event in
    switch event {
    case .bluetoothState(let state): print("Bluetooth: \(state)")
    case .scanning: print("Scanning…")
    case let .discovered(id, name, rssi, recognized):
        print("\(recognized ? "MATCH" : "FOUND") \(id) RSSI=\(rssi) \(name)")
    case .connecting(let id): print("Connecting: \(id)")
    case .connected(let id): print("Connected: \(id)")
    case .serviceAvailable(let id): print("Service verified: \(id)")
    case .pairing(let id): print("Pairing: \(id). Accept a macOS pairing prompt if shown.")
    case .paired(let id): print("PAIRED: \(id). Encrypted read succeeded; peripheral reports a stored bond. Ctrl-C to disconnect.")
    case let .disconnected(id, reason):
        print("Disconnected: \(id) \(reason ?? "")")
        exitCode = 1
    case .failed(let message):
        fputs("Error: \(message)\n", stderr)
        exitCode = 1
        finished = true
    case .rpcStatus(let status): print(status)
    case .stopped: finished = true
    }
}
signal(SIGINT, SIG_IGN)
signal(SIGTERM, SIG_IGN)
let signals = [SIGINT, SIGTERM].map { number -> DispatchSourceSignal in
    let source = DispatchSource.makeSignalSource(signal: number, queue: .main)
    source.setEventHandler { central.stop() }
    source.resume()
    return source
}
central.start(listOnly: options.listOnly, scanTimeout: options.timeout, connectTimeout: options.connectTimeout,
              verifyPairing: options.verifyPairing, pairingTimeout: options.pairingTimeout)
while !finished { RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.1)) }
withExtendedLifetime(signals) {}
exit(exitCode)
