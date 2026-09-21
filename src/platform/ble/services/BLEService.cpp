#include "BLEService.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
// Direct NimBLE use must reserve controller memory before Arduino initArduino().
#include <esp32-hal-bt-mem.h>
#include <esp_err.h>
#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include <array>
#include <atomic>
#include <cstring>

#include "../BLEProfile.h"

#if !defined(CONFIG_NIMBLE_ENABLED) || !CONFIG_BT_NIMBLE_NVS_PERSIST
#error "BLEService requires the SDK NimBLE host with persistent NVS bonding"
#endif

extern "C" void ble_store_config_init();

namespace platform::ble {

class BLEService::Backend {
   public:
    ~Backend() {
        stop();
    }

    bool start() {
        if (_running) return true;
        if (_active || nimble_port_init() != ESP_OK) return false;
        _active = this;
        _initialized = true;
        _ready = false;
        ble_hs_cfg.sync_cb = _onSync;
        ble_hs_cfg.reset_cb = _onReset;
        ble_hs_cfg.store_status_cb = [](ble_store_status_event*, void*) { return BLE_HS_ESTORE_CAP; };
        ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
        ble_hs_cfg.sm_bonding = 1;
        ble_hs_cfg.sm_mitm = 0;
        ble_hs_cfg.sm_sc = 1;
        ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
        ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
        ble_store_config_init();
        ble_svc_gap_init();
        ble_svc_gatt_init();
        if (!_registerService()) {
            stop();
            return false;
        }
        _running = true;
        _hostStarted = true;
        nimble_port_freertos_init(_hostTask);
        const auto started = millis();
        while (!_ready && millis() - started < 5000U) delay(1);
        if (_ready && _state != State::Failed) return true;
        stop();
        _state = State::Failed;
        return false;
    }

    void stop() {
        _running = false;
        if (_initialized) {
            // Stop joins the host before callback state and GATT definitions can be released.
            if (_hostStarted) ESP_ERROR_CHECK(nimble_port_stop());
            ESP_ERROR_CHECK(nimble_port_deinit());
            _hostStarted = false;
            _initialized = false;
            _active = nullptr;
        }
        _state = State::Stopped;
    }

    bool isRunning() const {
        return _running;
    }

    State state() const {
        return _state;
    }

    std::size_t bondCount() const {
        return _bondCount;
    }

   private:
    bool _registerService() {
        if (ble_svc_gap_device_name_set("Dayring") != 0) return false;
        if (ble_uuid_from_str(&_serviceUUID, serviceUUID) != 0 ||
            ble_uuid_from_str(&_statusUUID, pairingStatusUUID) != 0)
            return false;
        _characteristics = {};
        _characteristics[0].uuid = &_statusUUID.u;
        _characteristics[0].access_cb = _readStatus;
        _characteristics[0].arg = this;
        _characteristics[0].flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC;
        _characteristics[0].min_key_size = 16;
        _services = {};
        _services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
        _services[0].uuid = &_serviceUUID.u;
        _services[0].characteristics = _characteristics.data();
        return ble_gatts_count_cfg(_services.data()) == 0 && ble_gatts_add_svcs(_services.data()) == 0;
    }

    bool _advertise() {
        ble_hs_adv_fields fields{};
        fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
        fields.uuids128 = &_serviceUUID.u128;
        fields.num_uuids128 = 1;
        fields.uuids128_is_complete = 1;
        ble_hs_adv_fields response{};
        response.name = reinterpret_cast<const uint8_t*>("Dayring");
        response.name_len = 7;
        response.name_is_complete = 1;
        ble_gap_adv_params params{};
        params.conn_mode = BLE_GAP_CONN_MODE_UND;
        params.disc_mode = BLE_GAP_DISC_MODE_GEN;
        const bool success = ble_gap_adv_set_fields(&fields) == 0 && ble_gap_adv_rsp_set_fields(&response) == 0 &&
                             ble_gap_adv_start(_addressType, nullptr, BLE_HS_FOREVER, &params, _gapEvent, this) == 0;
        _state = success ? State::Advertising : State::Failed;
        return success;
    }

    bool _storedPeer(const ble_addr_t* peer = nullptr) {
        std::array<ble_addr_t, CONFIG_BT_NIMBLE_MAX_BONDS> peers{};
        int count = 0;
        if (ble_store_util_bonded_peers(peers.data(), &count, peers.size()) != 0) return false;
        _bondCount = count;
        for (int index = 0; peer && index < count; ++index) {
            if (ble_addr_cmp(&peers[index], peer) == 0) return true;
        }
        return false;
    }

    static void _hostTask(void*) {
        nimble_port_run();
        nimble_port_freertos_deinit();
    }

    static void _onSync() {
        auto& self = *_active;
        if (!self._running) return;
        self._storedPeer();
        if (ble_hs_util_ensure_addr(0) != 0 || ble_hs_id_infer_auto(0, &self._addressType) != 0 || !self._advertise())
            self._state = State::Failed;
        self._ready = true;
    }

    static void _onReset(int) {
        if (_active) _active->_state = State::Failed;
    }

    static int _gapEvent(ble_gap_event* event, void* context) {
        auto& self = *static_cast<Backend*>(context);
        if (!self._running) return 0;
        switch (event->type) {
            case BLE_GAP_EVENT_CONNECT:
                if (event->connect.status != 0) {
                    self._advertise();
                } else {
                    self._state = State::Connected;
                    const int result = ble_gap_security_initiate(event->connect.conn_handle);
                    if (result != 0 && result != BLE_HS_EALREADY)
                        ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                }
                break;
            case BLE_GAP_EVENT_DISCONNECT:
                self._advertise();
                break;
            case BLE_GAP_EVENT_ENC_CHANGE: {
                ble_gap_conn_desc connection{};
                if (event->enc_change.status == 0 &&
                    ble_gap_conn_find(event->enc_change.conn_handle, &connection) == 0 &&
                    connection.sec_state.encrypted && connection.sec_state.bonded) {
                    self._storedPeer();
                    self._state = State::Secured;
                } else {
                    self._state = State::Failed;
                    ble_gap_terminate(event->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                }
                break;
            }
            case BLE_GAP_EVENT_REPEAT_PAIRING:
                // Never silently replace an existing owner's bond.
                return BLE_GAP_REPEAT_PAIRING_IGNORE;
            default:
                break;
        }
        return 0;
    }

    static int _readStatus(uint16_t connectionHandle, uint16_t, ble_gatt_access_ctxt* access, void* context) {
        auto& self = *static_cast<Backend*>(context);
        ble_gap_conn_desc connection{};
        if (!self._running || ble_gap_conn_find(connectionHandle, &connection) != 0) return BLE_ATT_ERR_UNLIKELY;
        if (!connection.sec_state.encrypted) return BLE_ATT_ERR_INSUFFICIENT_ENC;
        const bool bonded = connection.sec_state.bonded && self._storedPeer(&connection.peer_id_addr);
        const char* response = bonded ? pairedResponse : "pending";
        return os_mbuf_append(access->om, response, std::strlen(response)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    static inline Backend* _active = nullptr;
    bool _initialized = false;
    bool _hostStarted = false;
    uint8_t _addressType = 0;
    ble_uuid_any_t _serviceUUID{};
    ble_uuid_any_t _statusUUID{};
    std::array<ble_gatt_chr_def, 2> _characteristics{};
    std::array<ble_gatt_svc_def, 2> _services{};
    std::atomic<bool> _running{false};
    std::atomic<bool> _ready{false};
    std::atomic<State> _state{State::Stopped};
    std::atomic<std::size_t> _bondCount{0};
};

}  // namespace platform::ble

#else

namespace platform::ble {

class BLEService::Backend {
   public:
    bool start() {
        _running = true;
        return true;
    }

    void stop() {
        _running = false;
    }

    bool isRunning() const {
        return _running;
    }

    State state() const {
        return _running ? State::Unavailable : State::Stopped;
    }

    std::size_t bondCount() const {
        return 0;
    }

   private:
    bool _running = false;
};

}  // namespace platform::ble

#endif

namespace platform::ble {

BLEService::BLEService() : _backend(std::make_unique<Backend>()) {
}

BLEService::~BLEService() = default;

bool BLEService::start() {
    return _backend->start();
}

void BLEService::stop() {
    _backend->stop();
}

bool BLEService::isRunning() const {
    return _backend->isRunning();
}

BLEService::State BLEService::state() const {
    return _backend->state();
}

std::size_t BLEService::bondCount() const {
    return _backend->bondCount();
}

}  // namespace platform::ble
