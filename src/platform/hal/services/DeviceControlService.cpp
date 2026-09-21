#include "DeviceControlService.h"

#include <Arduino.h>

#include "../Hardware.h"
#include "../Restart.h"

namespace platform::hal {

DeviceControlService::DeviceControlService(rpc::RPCService& rpc, ble::BLEService& ble) : _rpc(rpc), _ble(ble) {
}

DeviceControlService::~DeviceControlService() {
    stop();
}

bool DeviceControlService::start() {
    if (_running) return true;
    if (!_rpc.registerHandler(rpc::pairingResetMethod, [this](auto bytes) { return _resetPairing(bytes); }))
        return false;
    if (!_rpc.registerHandler(rpc::deviceRebootMethod, [this](auto bytes) { return _reboot(bytes); })) {
        _rpc.removeHandler(rpc::pairingResetMethod);
        return false;
    }
    _resetSession = 0;
    _resetCompleted = false;
    _restartPending = false;
    _running = true;
    return true;
}

void DeviceControlService::stop() {
    if (!_running) return;
    _running = false;
    _restartPending = false;
    _resetSession = 0;
    _rpc.removeHandler(rpc::pairingResetMethod);
    _rpc.removeHandler(rpc::deviceRebootMethod);
}

rpc::Reply DeviceControlService::_resetPairing(std::span<const uint8_t> payload) {
    if (payload.empty()) {
        if (_ble.bondResetState() == ble::BLEService::BondResetState::Failed) _resetSession = 0;
        if (_restartPending || _resetSession || !_ble.clearBonds()) return {.error = rpc::Error::Busy};
        _resetSession = _rpc.session();
        _resetCompleted = false;
        return {.payload = {0}};
    }
    if (payload.size() != 1 || payload[0] != 1 || !_resetSession || _resetSession != _rpc.session())
        return {.error = rpc::Error::InvalidPayload};
    switch (_ble.bondResetState()) {
        case ble::BLEService::BondResetState::Pending:
            return {.payload = {0}};
        case ble::BLEService::BondResetState::Complete:
            if (!_restartPending) _scheduleReboot();
            return {.payload = {1}};
        default:
            return {.error = rpc::Error::Internal};
    }
}

rpc::Reply DeviceControlService::_reboot(std::span<const uint8_t> payload) {
    if (!payload.empty()) return {.error = rpc::Error::InvalidPayload};
    if (_restartPending || _resetSession) return {.error = rpc::Error::Busy};
    _scheduleReboot();
    return {};
}

void DeviceControlService::_scheduleReboot() {
    // Leave time for the accepted response and pairing-reset status polling before restarting.
    _restartAt = static_cast<uint32_t>(millis());
    _restartPending = true;
}

void DeviceControlService::update(uint32_t) {
    if (!_running) return;
    if (_resetSession && !_restartPending && _ble.bondResetState() == ble::BLEService::BondResetState::Complete) {
        const auto now = static_cast<uint32_t>(millis());
        if (!_resetCompleted) {
            _resetCompleted = true;
            _resetCompletedAt = now;
        }
        // Let the requester fetch the final status before restarting; still finish an abandoned reset.
        if (_rpc.session() != _resetSession || now - _resetCompletedAt >= 5000U) _scheduleReboot();
    }
    if (_resetSession && _ble.bondResetState() == ble::BLEService::BondResetState::Failed) {
        _restartPending = false;
        if (_rpc.session() != _resetSession) _resetSession = 0;
    }
    if (!_restartPending || static_cast<uint32_t>(static_cast<uint32_t>(millis()) - _restartAt) < 1000U ||
        !displayReady())
        return;
    _restartPending = false;
    restartDevice();
}

}  // namespace platform::hal
