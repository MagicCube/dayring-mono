import Darwin
import DayringBLE
import Foundation

let arguments = Array(CommandLine.arguments.dropFirst())
if arguments == ["--help"] || arguments == ["-h"] ||
    (arguments.count == 2 && Command(rawValue: arguments[0]) != nil && ["--help", "-h"].contains(arguments[1])) {
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
let pairingStore = options.command == .resetPairing ? MacPairingStore() : nil
let administration: AdministrationRunner? = options.command == .devServer ? nil : AdministrationRunner(
    options: options, store: pairingStore,
    request: { method, payload, timeout, completion in
        central.requestRPC(method: method, payload: payload, timeout: timeout, completion: completion)
    }, stopLink: { central.stop() }, finish: { code, message in
        exitCode = code
        if code == 0 { print(message) } else { fputs("Error: \(message)\n", stderr) }
        finished = true
    })
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
        if let administration { administration.fail(message) } else { finished = true }
    case .rpcReady(let id): administration?.rpcReady(identifier: id)
    case .rpcStatus(let status): print(status)
    case .stopped: if administration == nil { finished = true }
    }
}
signal(SIGINT, SIG_IGN)
signal(SIGTERM, SIG_IGN)
let signals = [SIGINT, SIGTERM].map { number -> DispatchSourceSignal in
    let source = DispatchSource.makeSignalSource(signal: number, queue: .main)
    source.setEventHandler {
        if let administration { administration.fail("Interrupted") } else { central.stop() }
    }
    source.resume()
    return source
}
if !options.macOnly { central.start(listOnly: options.listOnly, scanTimeout: options.timeout, connectTimeout: options.connectTimeout,
              verifyPairing: options.verifyPairing, pairingTimeout: options.pairingTimeout) }
while !finished {
    RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.1))
    administration?.update()
}
withExtendedLifetime(signals) {}
exit(exitCode)
