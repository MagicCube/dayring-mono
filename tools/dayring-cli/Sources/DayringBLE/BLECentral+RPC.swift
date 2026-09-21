import CoreBluetooth
import Foundation

extension BLECentral {
    public var isRPCReady: Bool {
        dispatchPrecondition(condition: .onQueue(.main))
        return _rpcReady
    }

    public func peripheral(_ peripheral: CBPeripheral, didModifyServices invalidatedServices: [CBService]) {
        guard _running, !_reconnecting, peripheral === _peripheral,
              invalidatedServices.contains(where: { $0.uuid == CBUUID(nsuuid: _profile.serviceUUID) }) else { return }
        resetRPC()
        _pairingRetry?.cancel(); _pairingRetry = nil; _pairingCharacteristic = nil
        onEvent?(.rpcStatus("GATT services changed; rediscovering"))
        _deadline(after: _connectTimeout, message: "Service rediscovery timeout")
        peripheral.discoverServices([CBUUID(nsuuid: _profile.serviceUUID)])
    }

    func resetRPC() {
        _rpcEpoch &+= 1
        _rpcReady = false
        _rpcTimer?.invalidate(); _rpcTimer = nil
        _rpcWrite = nil; _rpcNotify = nil; _rpcQueue.removeAll()
        _rpc.reset()
    }

    @discardableResult
    public func requestRPC(method: UInt16, payload: Data = Data(), timeout: TimeInterval = 120,
                           completion: @escaping RPCPeer.Completion) -> UInt16? {
        dispatchPrecondition(condition: .onQueue(.main))
        guard _rpcReady else { completion(.failure(.unavailable)); return nil }
        return _rpc.request(method: method, payload: payload, timeout: timeout, completion: completion)
    }

    public func registerRPCHandler(method: UInt16, handler: @escaping RPCPeer.Handler) {
        dispatchPrecondition(condition: .onQueue(.main))
        precondition(method > 7, "Methods 0 through 7 are reserved")
        _rpc.register(method: method, handler: handler)
    }

    public func cancelRPC(_ id: UInt16) {
        dispatchPrecondition(condition: .onQueue(.main))
        _rpc.cancel(id)
    }

    func configureRPC(service: CBService, peripheral: CBPeripheral) {
        _rpcWrite = service.characteristics?.first { $0.uuid == CBUUID(nsuuid: DeviceProfile.rpcWriteUUID) && $0.properties.contains(.write) }
        _rpcNotify = service.characteristics?.first { $0.uuid == CBUUID(nsuuid: DeviceProfile.rpcNotifyUUID) && $0.properties.contains(.notify) }
    }

    func startRPC(_ peripheral: CBPeripheral) {
        guard let notify = _rpcNotify, _rpcWrite != nil else {
            onEvent?(.rpcStatus("RPC unavailable: firmware does not expose the RPC characteristics"))
            return
        }
        let epoch = _rpcEpoch
        _rpc.send = { [weak self] data in
            guard let self, self._running, self._rpcEpoch == epoch, !self._reconnecting,
                  let peripheral = self._peripheral, let write = self._rpcWrite,
                  self._rpcQueue.count < 8, data.count <= peripheral.maximumWriteValueLength(for: .withResponse) else { return false }
            self._rpcQueue.append(data)
            if self._rpcQueue.count == 1 {
                self._rpcWriteStarted = ProcessInfo.processInfo.systemUptime
                peripheral.writeValue(data, for: write, type: .withResponse)
            }
            return true
        }
        _rpc.maximumPacketSize = { [weak peripheral] in
            // Use the single-ATT-write limit even though writes request an acknowledgment.
            min(244, peripheral?.maximumWriteValueLength(for: .withoutResponse) ?? 20)
        }
        _rpc.onTransportFailure = { [weak self] in
            guard let self, self._running, self._rpcEpoch == epoch else { return }
            self.recoverConnection("RPC transfer stalled")
        }
        var zoneSnapshot = Data()
        var verifyAt: TimeInterval?
        _rpc.register(method: 2) { [weak self] payload in
            let zone = TimeZone(identifier: TimeZone.autoupdatingCurrent.identifier) ?? .current
            guard payload.isEmpty, let sample = ClockSample.encode(timeZone: zone) else { return .failure(.init(2)) }
            zoneSnapshot = Data(zone.identifier.utf8)
            self?.onEvent?(.rpcStatus("Clock sample served (UTC + offset, time zone: \(zone.identifier))"))
            return .success(sample)
        }
        _rpc.register(method: 3) { payload in
            guard payload.count == 1, let offset = payload.first, !zoneSnapshot.isEmpty,
                  Int(offset) <= zoneSnapshot.count else { return .failure(.init(2)) }
            let chunk = Data(zoneSnapshot.dropFirst(Int(offset)).prefix(12))
            if chunk.count < 12 { verifyAt = ProcessInfo.processInfo.systemUptime + 1 }
            return .success(chunk)
        }
        let zoneMonitor = TimeZoneMonitor()
        _rpcTimer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
            guard let self, self._running, self._rpcEpoch == epoch else { return }
            let writeTimeout: TimeInterval = 5
            if !self._rpcQueue.isEmpty && ProcessInfo.processInfo.systemUptime - self._rpcWriteStarted >= writeTimeout {
                self.recoverConnection("RPC write acknowledgment timeout after \(writeTimeout)s (\(self._rpcReady ? "ready" : "handshake"))")
                return
            }
            self._rpc.poll()
            guard self._running, self._rpcEpoch == epoch else { return }
            if let deadline = verifyAt, ProcessInfo.processInfo.systemUptime >= deadline {
                verifyAt = nil
                self.verifyClockStatus()
            }
            zoneMonitor.poll(now: ProcessInfo.processInfo.systemUptime) { [weak self] completed in
                guard let self else { completed(false); return }
                self._rpc.request(method: 4) { [weak self] result in
                    if case .success = result {
                        self?.onEvent?(.rpcStatus("Time zone changed; device requested to resynchronize"))
                        completed(true)
                    } else { completed(false) }
                }
            }
        }
        _deadline(after: _connectTimeout, message: "RPC notification subscription timeout")
        peripheral.setNotifyValue(true, for: notify)
    }

    public func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        guard _running, !_reconnecting, peripheral === _peripheral, characteristic === _rpcNotify else { return }
        if let error { _fail("RPC subscription failed: \(error.localizedDescription)"); return }
        guard characteristic.isNotifying else { _fail("RPC notifications disabled"); return }
        _timer?.invalidate(); _timer = nil
        beginRPCHandshake()
    }

    func beginRPCHandshake() {
        let epoch = _rpcEpoch
        _rpc.request(method: 0, payload: Data([2]), timeout: 5) { [weak self] result in
            guard let self, self._running, self._rpcEpoch == epoch else { return }
            guard case .success(let capabilities) = result else {
                if case .failure(.remote(2)) = result {
                    self._fail("RPC protocol mismatch: firmware must support 10 KiB messages")
                } else { self.recoverConnection("RPC handshake failed: \(result)") }
                return
            }
            guard capabilities == Data([2]) else {
                self._fail("RPC protocol mismatch: firmware must support 10 KiB messages")
                return
            }
            self._rpcReady = true
            self._recovery.connected()
            self.onEvent?(.rpcStatus("RPC ready"))
            if let peripheral = self._peripheral { self.onEvent?(.rpcReady(peripheral.identifier)) }
            guard self._running, self._rpcEpoch == epoch else { return }
            self._rpc.request(method: 1, payload: Data("ping".utf8)) { [weak self] result in
                guard let self, self._running, self._rpcEpoch == epoch else { return }
                switch result {
                case .success(let data) where data == Data("ping".utf8): self.onEvent?(.rpcStatus("RPC ping succeeded"))
                default: self.onEvent?(.rpcStatus("RPC ping failed: \(result)"))
                }
            }
        }
    }

    func verifyClockStatus() {
        let epoch = _rpcEpoch
        _rpc.request(method: 5) { [weak self] result in
            guard let self, self._running, self._rpcEpoch == epoch else { return }
            guard case .success(let data) = result, data.count == 12 else {
                self.onEvent?(.rpcStatus("RTC status read failed: \(result)")); return
            }
            let bytes = Array(data)
            guard bytes[0] == 2 else {
                self.onEvent?(.rpcStatus("RTC has not synchronized yet (state=\(bytes[0]))")); return
            }
            let year = Int(bytes[1]) | Int(bytes[2]) << 8
            let raw = UInt32(bytes[8]) | UInt32(bytes[9]) << 8 | UInt32(bytes[10]) << 16 | UInt32(bytes[11]) << 24
            let stamp = String(format: "%04d-%02d-%02d %02d:%02d:%02d", year, Int(bytes[3]), Int(bytes[4]), Int(bytes[5]), Int(bytes[6]), Int(bytes[7]))
            self.onEvent?(.rpcStatus("RTC synchronized: \(stamp), UTC offset \(Int32(bitPattern: raw)) seconds (device readback)"))
        }
    }

    public func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard _running, !_reconnecting, peripheral === _peripheral, characteristic === _rpcWrite else { return }
        if let error { recoverConnection("RPC write failed: \(error.localizedDescription)"); return }
        guard !_rpcQueue.isEmpty else { return }
        let elapsed = ProcessInfo.processInfo.systemUptime - _rpcWriteStarted
        if elapsed >= 1 {
            onEvent?(.rpcStatus(String(format: "RPC write acknowledged after %.2fs", elapsed)))
        }
        _rpcQueue.removeFirst()
        if let next = _rpcQueue.first {
            _rpcWriteStarted = ProcessInfo.processInfo.systemUptime
            peripheral.writeValue(next, for: characteristic, type: .withResponse)
        }
    }
}
