import CoreBluetooth
import Foundation

/// Own on the main queue. All methods and events use the main queue on macOS and iOS.
/// Retain this object until stop() completes. Connection does not imply bonding or RPC readiness.
public final class BLECentral: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    public var onEvent: ((CentralEvent) -> Void)?
    let _profile: DeviceProfile
    var _manager: CBCentralManager!
    var _peripheral: CBPeripheral?
    var _timer: Timer?
    var _pairingRetry: DispatchWorkItem?
    var _pairingCharacteristic: CBCharacteristic?
    private var _verifyPairing = true
    private var _pairingTimeout: TimeInterval = 60
    var _running = false
    private var _listOnly = false
    var _connectTimeout: TimeInterval = 15
    private var _scanTimeout: TimeInterval = 20
    private var _seen = Set<UUID>()
    let _rpc = RPCPeer()
    var _rpcWrite: CBCharacteristic?
    var _rpcNotify: CBCharacteristic?
    var _rpcQueue: [Data] = []
    var _rpcWriteStarted: TimeInterval = 0
    var _rpcTimer: Timer?
    var _rpcEpoch: UInt64 = 0
    var _rpcReady = false
    var _reconnecting = false
    var _reconnectWork: DispatchWorkItem?
    var _recovery = ConnectionRecovery()

    public init(profile: DeviceProfile = DeviceProfile()) {
        _profile = profile
        super.init()
    }

    public func start(listOnly: Bool = false, scanTimeout: TimeInterval = 20,
                      connectTimeout: TimeInterval = 15, verifyPairing: Bool = true, pairingTimeout: TimeInterval = 60) {
        dispatchPrecondition(condition: .onQueue(.main))
        guard !_running else { return }
        precondition(scanTimeout.isFinite && scanTimeout > 0 && connectTimeout.isFinite && connectTimeout > 0)
        precondition(pairingTimeout.isFinite && pairingTimeout > 0)
        _verifyPairing = verifyPairing
        _pairingTimeout = pairingTimeout
        _running = true
        _recovery.connected()
        _listOnly = listOnly
        _scanTimeout = scanTimeout
        _connectTimeout = connectTimeout
        _seen.removeAll()
        _deadline(after: scanTimeout, message: "Bluetooth did not become available before timeout")
        if _manager == nil {
            _manager = CBCentralManager(delegate: self, queue: .main)
        } else {
            centralManagerDidUpdateState(_manager)
        }
    }

    public func stop() {
        dispatchPrecondition(condition: .onQueue(.main))
        guard _running else { return }
        _running = false
        _reconnectWork?.cancel(); _reconnectWork = nil
        _reconnecting = false
        resetRPC()
        _timer?.invalidate()
        _timer = nil
        _pairingRetry?.cancel()
        _pairingRetry = nil
        _pairingCharacteristic = nil
        _manager?.stopScan()
        if let peripheral = _peripheral {
            peripheral.delegate = nil
            _manager.cancelPeripheralConnection(peripheral)
        }
        _peripheral = nil
        onEvent?(.stopped)
    }

    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        guard _running else { return }
        let names: [CBManagerState: String] = [
            .unknown: "unknown", .resetting: "resetting", .unsupported: "unsupported",
            .unauthorized: "unauthorized", .poweredOff: "poweredOff", .poweredOn: "poweredOn",
        ]
        onEvent?(.bluetoothState(names[central.state] ?? "unknown"))
        switch central.state {
        case .poweredOn:
            guard _peripheral == nil, !central.isScanning else { return }
            central.scanForPeripherals(withServices: _listOnly ? nil : [CBUUID(nsuuid: _profile.serviceUUID)])
            _deadline(after: _scanTimeout, message: "Scan timeout: no matching peripheral connected")
            onEvent?(.scanning)
        case .unknown, .resetting:
            if _peripheral != nil { _fail("Bluetooth reset interrupted the session") }
        default:
            _fail("Bluetooth unavailable: \(names[central.state] ?? "unknown"). Check Bluetooth and privacy settings.")
        }
    }

    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                               advertisementData: [String: Any], rssi RSSI: NSNumber) {
        guard _running, _peripheral == nil else { return }
        let services = (advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? [])
            + (advertisementData[CBAdvertisementDataOverflowServiceUUIDsKey] as? [CBUUID] ?? [])
        let recognized = _profile.matches(identifier: peripheral.identifier, advertisedServices: services)
        if _seen.insert(peripheral.identifier).inserted {
            let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name ?? "Unnamed"
            onEvent?(.discovered(id: peripheral.identifier, name: name, rssi: RSSI.intValue, recognized: recognized))
        }
        guard !_listOnly, recognized,
              (advertisementData[CBAdvertisementDataIsConnectable] as? NSNumber)?.boolValue != false else { return }
        _peripheral = peripheral
        peripheral.delegate = self
        central.stopScan()
        _deadline(after: _connectTimeout, message: "Connection timeout")
        central.connect(peripheral)
        onEvent?(.connecting(peripheral.identifier))
    }

    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard _running, !_reconnecting, peripheral === _peripheral else { return }
        _deadline(after: _connectTimeout, message: "Service discovery timeout")
        peripheral.discoverServices([CBUUID(nsuuid: _profile.serviceUUID)])
        onEvent?(.connected(peripheral.identifier))
    }

    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard _running, !_reconnecting, peripheral === _peripheral else { return }
        if let error { _fail("Service discovery failed: \(error.localizedDescription)"); return }
        guard let service = peripheral.services?.first(where: { $0.uuid == CBUUID(nsuuid: _profile.serviceUUID) }) else {
            _fail("Connected peripheral does not expose the expected service")
            return
        }
        _timer?.invalidate()
        _timer = nil
        onEvent?(.serviceAvailable(peripheral.identifier))
        if _verifyPairing {
            _deadline(after: _pairingTimeout, message: "Pairing timeout: encrypted bond confirmation not received")
            peripheral.discoverCharacteristics([CBUUID(nsuuid: DeviceProfile.pairingStatusUUID),
                CBUUID(nsuuid: DeviceProfile.rpcWriteUUID), CBUUID(nsuuid: DeviceProfile.rpcNotifyUUID)], for: service)
            onEvent?(.pairing(peripheral.identifier))
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService,
                           error: Error?) {
        guard _running, !_reconnecting, peripheral === _peripheral else { return }
        if let error { _fail("Pairing characteristic discovery failed: \(error.localizedDescription)"); return }
        guard let characteristic = service.characteristics?.first(where: {
            $0.uuid == CBUUID(nsuuid: DeviceProfile.pairingStatusUUID) && $0.properties.contains(.read)
        }) else {
            _fail("Pairing status characteristic is missing or not readable")
            return
        }
        configureRPC(service: service, peripheral: peripheral)
        _pairingCharacteristic = characteristic
        peripheral.readValue(for: characteristic)
    }

    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic,
                           error: Error?) {
        guard _running, !_reconnecting, peripheral === _peripheral else { return }
        if characteristic === _rpcNotify {
            if let error { _fail("RPC notification failed: \(error.localizedDescription)"); return }
            do { if let data = characteristic.value { try _rpc.receive(data) } }
            catch { _fail("Invalid RPC message: \(error)") }
            return
        }
        guard characteristic === _pairingCharacteristic else { return }
        if let error { _fail("Encrypted pairing read failed: \(error.localizedDescription)"); return }
        if characteristic.value == DeviceProfile.pairedResponse {
            _timer?.invalidate()
            _timer = nil
            _pairingCharacteristic = nil
            onEvent?(.paired(peripheral.identifier))
            startRPC(peripheral)
        } else if characteristic.value == Data("pending".utf8) {
            // Encryption can complete just before the host finishes storing the bond.
            let retry = DispatchWorkItem { [weak self] in
                guard let self, self._running, let peer = self._peripheral,
                      let status = self._pairingCharacteristic else { return }
                peer.readValue(for: status)
            }
            _pairingRetry = retry
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.25, execute: retry)
        } else {
            _fail("Unexpected pairing status response")
        }
    }

    public func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        guard _running, !_reconnecting, peripheral === _peripheral else { return }
        recoverConnection("Connection failed: \(error?.localizedDescription ?? "unknown error")")
    }

    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral,
                               error: Error?) {
        guard _running, peripheral === _peripheral else { return }
        if _reconnecting {
            scheduleReconnect(peripheral)
        } else {
            recoverConnection("Link disconnected: \(error?.localizedDescription ?? "remote disconnect")")
        }
    }

    func _deadline(after seconds: TimeInterval, message: String) {
        _timer?.invalidate()
        _timer = Timer.scheduledTimer(withTimeInterval: seconds, repeats: false) { [weak self] _ in
            guard let self, self._running else { return }
            if self._listOnly, self._manager?.isScanning == true { self.stop() }
            else { self._fail(message) }
        }
    }

    func _fail(_ message: String) {
        guard _running else { return }
        stop()
        onEvent?(.failed(message))
    }
}
