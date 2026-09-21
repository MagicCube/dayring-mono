#include "BLEService.h"

#include "../RPCMailbox.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
// Direct NimBLE use must reserve controller memory before Arduino initArduino().
#include <esp32-hal-bt-mem.h>
#include <esp_err.h>
#include <host/ble_hs.h>
#include <host/ble_hs_mbuf.h>
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
        _retryAdvertising = false;
        _retryAt = millis();
        ble_npl_event_init(&_retryEvent, _retry, this);
        _connection = UINT16_MAX;
        _subscribed = false;
        _secure = false;
        ble_npl_event_init(&_txEvent, _transmit, this);
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

    void update(uint32_t now) {
        if (!_running || !_retryAdvertising || now - _retryAt < 1000U) return;
        _retryAt = now;
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &_retryEvent);
    }

    void stop() {
        _running = false;
        _retryAdvertising = false;
        _mailbox.reset(false);
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

    uint32_t session() const {
        return _mailbox.session();
    }

    bool receive(rpc::Packet& packet) {
        return _mailbox.receive(packet);
    }

    bool send(uint32_t session, std::span<const uint8_t> bytes) {
        if (!_running || !_mailbox.send(session, bytes)) return false;
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &_txEvent);
        return true;
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
        if (ble_uuid_from_str(&_writeUUID, rpcWriteUUID) || ble_uuid_from_str(&_notifyUUID, rpcNotifyUUID))
            return false;
        _characteristics[1].uuid = &_writeUUID.u;
        _characteristics[1].access_cb = _writeRPC;
        _characteristics[1].arg = this;
        _characteristics[1].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC;
        _characteristics[1].min_key_size = 16;
        _characteristics[2].uuid = &_notifyUUID.u;
        _characteristics[2].access_cb = _readStatus;
        _characteristics[2].arg = this;
        _characteristics[2].flags = BLE_GATT_CHR_F_NOTIFY;
        _characteristics[2].val_handle = &_notifyHandle;
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
        _retryAdvertising = !success;
        _state = success ? State::Advertising : State::Failed;
        return success;
    }

    static void _retry(ble_npl_event* event) {
        auto& self = *static_cast<Backend*>(ble_npl_event_get_arg(event));
        if (self._running && self._retryAdvertising) self._advertise();
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
        if (_active) {
            _active->_retryAdvertising = false;
            _active->_connection = UINT16_MAX;
            _active->_secure = false;
            _active->_subscribed = false;
            _active->_mailbox.reset(false);
            _active->_state = State::Failed;
        }
    }

    static int _gapEvent(ble_gap_event* event, void* context) {
        auto& self = *static_cast<Backend*>(context);
        if (!self._running) return 0;
        switch (event->type) {
            case BLE_GAP_EVENT_CONNECT:
                if (event->connect.status != 0) {
                    self._advertise();
                } else {
                    self._beginConnection(event->connect.conn_handle);
                    if (!self._secure) {
                        const int result = ble_gap_security_initiate(event->connect.conn_handle);
                        if (result != 0 && result != BLE_HS_EALREADY)
                            ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                    }
                }
                break;
            case BLE_GAP_EVENT_DISCONNECT:
                self._connection = UINT16_MAX;
                self._mailbox.reset(false);
                self._secure = false;
                self._subscribed = false;
                self._advertise();
                break;
            case BLE_GAP_EVENT_ENC_CHANGE: {
                self._beginConnection(event->enc_change.conn_handle);
                ble_gap_conn_desc connection{};
                if (event->enc_change.status == 0 &&
                    ble_gap_conn_find(event->enc_change.conn_handle, &connection) == 0 &&
                    connection.sec_state.encrypted && connection.sec_state.bonded) {
                    self._storedPeer();
                    self._state = State::Secured;
                    self._secure = true;
                    // Bonded Apple centrals can retain the pre-upgrade GATT layout.
                    ble_svc_gatt_changed(1, 0xFFFF);
                    self._refreshSession();
                } else {
                    self._mailbox.reset(false);
                    self._secure = false;
                    self._state = State::Failed;
                    ble_gap_terminate(event->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                }
                break;
            }
            case BLE_GAP_EVENT_ADV_COMPLETE:
                self._advertise();
                break;
            case BLE_GAP_EVENT_SUBSCRIBE:
                if (event->subscribe.attr_handle == self._notifyHandle) {
                    if (event->subscribe.cur_notify) self._beginConnection(event->subscribe.conn_handle);
                    if (event->subscribe.conn_handle != self._connection) break;
                    self._subscribed = event->subscribe.cur_notify;
                    self._refreshSession();
                }
                break;
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

    void _beginConnection(uint16_t handle) {
        // Bond restoration can deliver ENC_CHANGE/SUBSCRIBE before CONNECT.
        // Initialize once per link; a late CONNECT must preserve its ready session.
        if (_connection == handle) return;
        _connection = handle;
        _retryAdvertising = false;
        _secure = false;
        _subscribed = false;
        _mailbox.reset(false);
        _state = State::Connected;
    }

    void _refreshSession() {
        const bool ready = _secure && _subscribed;
        if (ready != (_mailbox.session() != 0)) _mailbox.reset(ready);
    }

    static int _writeRPC(uint16_t handle, uint16_t, ble_gatt_access_ctxt* access, void* context) {
        auto& self = *static_cast<Backend*>(context);
        if (!self._running || !self._secure || handle != self._connection || !self._mailbox.session())
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        std::array<uint8_t, 20> bytes{};
        uint16_t length = 0;
        if (OS_MBUF_PKTLEN(access->om) > bytes.size() ||
            ble_hs_mbuf_to_flat(access->om, bytes.data(), bytes.size(), &length) != 0 || length < 8)
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        const bool delivered = self._mailbox.deliver({bytes.data(), length});
        return delivered ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    static void _transmit(ble_npl_event* event) {
        auto& self = *static_cast<Backend*>(ble_npl_event_get_arg(event));
        rpc::Packet packet;
        for (int budget = 0; budget < 8 && self._mailbox.transmit(packet); ++budget) {
            if (!self._running || !packet.session || packet.session != self._mailbox.session()) continue;
            auto* buffer = ble_hs_mbuf_from_flat(packet.bytes.data(), packet.size);
            if (!buffer || ble_gatts_notify_custom(self._connection, self._notifyHandle, buffer) != 0) {
                self._mailbox.reset(false);
                ble_gap_terminate(self._connection, BLE_ERR_REM_USER_CONN_TERM);
                break;
            }
        }
    }

    static inline Backend* _active = nullptr;
    bool _initialized = false;
    bool _hostStarted = false;
    uint8_t _addressType = 0;
    ble_uuid_any_t _serviceUUID{};
    ble_uuid_any_t _statusUUID{};
    ble_uuid_any_t _writeUUID{}, _notifyUUID{};
    uint16_t _notifyHandle = 0, _connection = UINT16_MAX;
    bool _secure = false, _subscribed = false;
    ble_npl_event _txEvent{}, _retryEvent{};
    std::atomic<bool> _retryAdvertising{false};
    uint32_t _retryAt = 0;
    RPCMailbox _mailbox;
    std::array<ble_gatt_chr_def, 4> _characteristics{};
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
    void update(uint32_t) {
    }

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

    uint32_t session() const {
        return 0;
    }

    bool receive(rpc::Packet&) {
        return false;
    }

    bool send(uint32_t, std::span<const uint8_t>) {
        return false;
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

uint32_t BLEService::session() const {
    return _backend->session();
}

bool BLEService::receive(rpc::Packet& packet) {
    return _backend->receive(packet);
}

bool BLEService::send(uint32_t session, std::span<const uint8_t> bytes) {
    return _backend->send(session, bytes);
}

bool BLEService::start() {
    return _backend->start();
}

void BLEService::update(uint32_t now) {
    _backend->update(now);
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
