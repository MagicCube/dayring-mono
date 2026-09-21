import CoreBluetooth
import Foundation
import ObjectiveC

/// Development-only macOS adapter. Private selectors are deliberately kept out of DayringBLE/iOS.
final class MacPairingStore: NSObject, CBCentralManagerDelegate, PairingStore {
    enum Failure: Error, CustomStringConvertible {
        case unavailable(String)
        var description: String {
            switch self { case .unavailable(let reason): return "macOS pairing removal unavailable: \(reason). Use System Settings > Bluetooth > Forget This Device." }
        }
    }
    private var _manager: CBCentralManager!
    private var _agent: NSObject?
    private var _peer: CBPeer?
    private let _paired = NSSelectorFromString("isPeerPaired:")
    private let _remove = NSSelectorFromString("unpairPeer:")

    override init() {
        super.init()
        _manager = CBCentralManager(delegate: self, queue: .main)
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {}
    var isReady: Bool { _manager.state == .poweredOn }

    func prepare(identifier: UUID) throws {
        guard isReady else { throw Failure.unavailable("Bluetooth is not ready") }
        let shared = NSSelectorFromString("sharedPairingAgent")
        guard Self._supports(_manager, shared, arguments: 2, result: "@"),
              let agent = _manager.perform(shared)?.takeUnretainedValue() as? NSObject,
              Self._supports(agent, _paired, arguments: 3, result: "B"),
              Self._supports(agent, _remove, arguments: 3, result: "v") else {
            throw Failure.unavailable("this OS does not expose the expected pairing-agent interface")
        }
        guard let peer = _manager.retrievePeripherals(withIdentifiers: [identifier]).first else {
            throw Failure.unavailable("cannot resolve the exact peripheral UUID \(identifier)")
        }
        _agent = agent
        _peer = peer
    }

    func isPaired() throws -> Bool {
        guard let agent = _agent, let peer = _peer else { throw Failure.unavailable("no exact device selected") }
        typealias Query = @convention(c) (AnyObject, Selector, AnyObject) -> Bool
        let query = unsafeBitCast(agent.method(for: _paired), to: Query.self)
        return query(agent, _paired, peer)
    }

    func remove() throws {
        guard let agent = _agent, let peer = _peer else { throw Failure.unavailable("no exact device selected") }
        if try !isPaired() { return }
        typealias Remove = @convention(c) (AnyObject, Selector, AnyObject) -> Void
        let remove = unsafeBitCast(agent.method(for: _remove), to: Remove.self)
        remove(agent, _remove, peer)
    }

    private static func _supports(_ object: NSObject, _ selector: Selector, arguments: UInt32, result: String) -> Bool {
        guard object.responds(to: selector), let cls = object_getClass(object),
              let method = class_getInstanceMethod(cls, selector), method_getNumberOfArguments(method) == arguments else { return false }
        let type = method_copyReturnType(method)
        defer { free(type) }
        guard String(cString: type) == result else { return false }
        for index in 2..<arguments {
            guard let argumentType = method_copyArgumentType(method, index) else { return false }
            defer { free(argumentType) }
            guard argumentType.pointee == 64 else { return false } // Objective-C object argument.
        }
        return true
    }
}
