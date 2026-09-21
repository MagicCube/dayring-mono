import CoreBluetooth
import Foundation

extension BLECentral {
    func recoverConnection(_ reason: String) {
        guard _running, !_reconnecting else { return }
        guard let peripheral = _peripheral, _recovery.nextDelay() != nil else {
            _fail(reason)
            return
        }
        // Invalidate callbacks before reset completes outstanding requests.
        _reconnecting = true
        resetRPC()
        _pairingRetry?.cancel(); _pairingRetry = nil
        _pairingCharacteristic = nil
        _deadline(after: _connectTimeout, message: "Recovery disconnect timeout: \(reason)")
        onEvent?(.rpcStatus("\(reason); reconnecting (\(_recovery.attempts)/2)"))
        guard _running else { return }
        if peripheral.state == .disconnected {
            scheduleReconnect(peripheral)
        } else {
            _manager.cancelPeripheralConnection(peripheral)
        }
    }

    func scheduleReconnect(_ peripheral: CBPeripheral) {
        _timer?.invalidate(); _timer = nil
        _reconnectWork?.cancel()
        let epoch = _rpcEpoch
        let work = DispatchWorkItem { [weak self, weak peripheral] in
            guard let self, let peripheral, self._running, self._reconnecting,
                  self._rpcEpoch == epoch, peripheral === self._peripheral else { return }
            self._reconnectWork = nil
            self._reconnecting = false
            self._deadline(after: self._connectTimeout, message: "Reconnection timeout")
            self._manager.connect(peripheral)
            self.onEvent?(.connecting(peripheral.identifier))
        }
        _reconnectWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + TimeInterval(_recovery.attempts), execute: work)
    }
}
