import DayringBLE
import Foundation

protocol PairingStore: AnyObject {
    var isReady: Bool { get }
    func prepare(identifier: UUID) throws
    func isPaired() throws -> Bool
    func remove() throws
}

final class AdministrationRunner {
    typealias Request = (UInt16, Data, TimeInterval, @escaping RPCPeer.Completion) -> Void
    private enum Phase { case waiting, clearing, forgetting, finished }
    private let _options: Options
    private let _store: PairingStore?
    private let _request: Request
    private let _stopLink: () -> Void
    private let _finish: (Int32, String) -> Void
    private let _now: () -> TimeInterval
    private var _phase = Phase.waiting
    private var _deadline: TimeInterval = 0
    private var _nextPoll: TimeInterval = 0
    private var _requestPending = false
    private var _resetStarted = false
    private var _espCleared = false

    init(options: Options, store: PairingStore?, request: @escaping Request, stopLink: @escaping () -> Void,
         now: @escaping () -> TimeInterval = { ProcessInfo.processInfo.systemUptime },
         finish: @escaping (Int32, String) -> Void) {
        _options = options; _store = store; _request = request; _stopLink = stopLink; _now = now; _finish = finish
        _deadline = now() + options.timeout
    }

    func rpcReady(identifier: UUID) {
        guard _phase == .waiting, !_requestPending, !_options.macOnly else { return }
        guard identifier == _options.device else { fail("Connected device does not match --device"); return }
        _deadline = _now() + _options.timeout
        if _options.command == .reboot {
            _requestPending = true
            _request(DeviceAdministration.rebootMethod, Data(), 5) { [weak self] result in
                guard let self, self._phase != .finished else { return }
                switch result {
                case .success(let data) where data.isEmpty:
                    self._complete("ESP32 restart accepted; it will restart after the current display refresh finishes.")
                case .success: self.fail("Invalid reboot response; restart outcome is unknown")
                case .failure(let error): self.fail("Reboot RPC failed: \(error); restart outcome is unknown")
                }
            }
            return
        }
        do {
            try _store?.prepare(identifier: identifier)
            guard let store = _store, try store.isPaired() else {
                fail("Cannot verify the selected macOS bond before clearing ESP32; no reset was sent")
                return
            }
            _phase = .clearing
            _resetStarted = true
            _pollReset(initial: true)
        } catch { fail(String(describing: error)) }
    }

    func update() {
        guard _phase != .finished else { return }
        // Connection discovery has its own deadlines in BLECentral.
        if _phase == .waiting && !_options.macOnly && !_requestPending { return }
        if _now() >= _deadline {
            fail(_phase == .forgetting ? "macOS unpairing was not confirmed; use System Settings > Bluetooth > Forget This Device"
                 : "Administrative operation timed out")
            return
        }
        if _options.macOnly && _phase == .waiting && _store?.isReady == true {
            do {
                try _store?.prepare(identifier: _options.device!)
                try _forgetMac()
            } catch { fail(String(describing: error)) }
        } else if _phase == .clearing && !_requestPending && _now() >= _nextPoll {
            _pollReset(initial: false)
        } else if _phase == .forgetting {
            do {
                if try _store?.isPaired() == false {
                    _complete(_espCleared ? "ESP32 bonds cleared and macOS bond removed; ESP32 restart scheduled."
                              : "Selected macOS bond removed; ESP32 was not contacted.")
                }
            } catch { fail(String(describing: error)) }
        }
    }

    func fail(_ message: String) {
        guard _phase != .finished else { return }
        _phase = .finished
        _stopLink()
        var detail = message
        if _resetStarted {
            detail += _espCleared ? ". ESP32 is cleared, but macOS removal was not confirmed."
                                  : ". ESP32 reset outcome is unconfirmed; macOS pairing was retained."
            detail += " Recover the local bond with: dayring-cli reset-pairing --device \(_options.device!) --mac-only"
        }
        _finish(1, detail)
    }

    private func _pollReset(initial: Bool) {
        _requestPending = true
        _request(DeviceAdministration.pairingResetMethod, initial ? Data() : Data([1]), 5) { [weak self] result in
            guard let self, self._phase == .clearing else { return }
            self._requestPending = false
            do {
                switch try DeviceAdministration.decodeResetState(result.get()) {
                case .pending: self._nextPoll = self._now() + 0.1
                case .complete:
                    self._espCleared = true
                    try self._forgetMac()
                }
            } catch { self.fail("Pairing reset RPC failed: \(error)") }
        }
    }

    private func _forgetMac() throws {
        _phase = .forgetting
        _stopLink() // Suppress automatic reconnect before either side drops its bond.
        try _store?.remove()
    }

    private func _complete(_ message: String) {
        _phase = .finished
        _stopLink()
        _finish(0, message)
    }
}
