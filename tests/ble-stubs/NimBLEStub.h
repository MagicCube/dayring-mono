#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#define CONFIG_NIMBLE_ENABLED 1
#define CONFIG_BT_NIMBLE_NVS_PERSIST 1
#define CONFIG_BT_NIMBLE_MAX_BONDS 3
#define ESP_OK 0
#define ESP_ERROR_CHECK(expression) assert((expression) == 0)
constexpr int BLE_HS_ESTORE_CAP = 1, BLE_HS_EALREADY = 2, BLE_HS_IO_NO_INPUT_OUTPUT = 3;
constexpr int BLE_SM_PAIR_KEY_DIST_ENC = 1, BLE_SM_PAIR_KEY_DIST_ID = 2;
constexpr int BLE_GATT_CHR_F_READ = 1, BLE_GATT_CHR_F_READ_ENC = 2, BLE_GATT_SVC_TYPE_PRIMARY = 1;
constexpr int BLE_HS_ADV_F_DISC_GEN = 2, BLE_HS_ADV_F_BREDR_UNSUP = 4, BLE_HS_FOREVER = -1;
constexpr int BLE_GAP_CONN_MODE_UND = 1, BLE_GAP_DISC_MODE_GEN = 1, BLE_ERR_REM_USER_CONN_TERM = 19;
constexpr int BLE_GAP_EVENT_ADV_COMPLETE = 6;
constexpr int BLE_GAP_EVENT_CONNECT = 1, BLE_GAP_EVENT_DISCONNECT = 2, BLE_GAP_EVENT_ENC_CHANGE = 3;
constexpr int BLE_GAP_EVENT_REPEAT_PAIRING = 4, BLE_GAP_REPEAT_PAIRING_IGNORE = 1;
constexpr int BLE_ATT_ERR_UNLIKELY = 14, BLE_ATT_ERR_INSUFFICIENT_ENC = 15, BLE_ATT_ERR_INSUFFICIENT_RES = 17;

struct ble_uuid_t {};

struct ble_uuid128_t {};

struct ble_uuid_any_t {
    ble_uuid_t u;
    ble_uuid128_t u128;
};

struct ble_addr_t {
    uint8_t type = 0;
    uint8_t val[6]{};
};

struct os_mbuf {
    std::string value;
};

struct ble_gatt_access_ctxt {
    os_mbuf* om;
};

struct ble_store_status_event {};

struct ble_gatt_chr_def {
    ble_uuid_t* uuid = nullptr;
    int (*access_cb)(uint16_t, uint16_t, ble_gatt_access_ctxt*, void*) = nullptr;
    void* arg = nullptr;
    int flags = 0;
    int min_key_size = 0;
    uint16_t* val_handle = nullptr;
};

struct ble_gatt_svc_def {
    int type = 0;
    ble_uuid_t* uuid = nullptr;
    ble_gatt_chr_def* characteristics = nullptr;
};

struct ble_hs_adv_fields {
    int flags = 0;
    ble_uuid128_t* uuids128 = nullptr;
    int num_uuids128 = 0, uuids128_is_complete = 0;
    const uint8_t* name = nullptr;
    int name_len = 0, name_is_complete = 0;
};

struct ble_gap_adv_params {
    int conn_mode = 0, disc_mode = 0;
};

struct ble_gap_conn_desc {
    struct {
        bool encrypted = false, bonded = false;
    } sec_state;

    ble_addr_t peer_id_addr;
};

struct ble_gap_event {
    int type = 0;

    struct {
        int status = 0;
        uint16_t conn_handle = 1;
    } connect, enc_change;

    struct {
        uint16_t conn_handle = 1;
        uint16_t attr_handle = 77;
        bool cur_notify = true;
    } subscribe;
};

struct HostConfig {
    void (*sync_cb)() = nullptr;
    void (*reset_cb)(int) = nullptr;
    int (*store_status_cb)(ble_store_status_event*, void*) = nullptr;
    int sm_io_cap = 0, sm_bonding = 0, sm_mitm = 0, sm_sc = 0, sm_our_key_dist = 0, sm_their_key_dist = 0;
};

inline HostConfig ble_hs_cfg;

namespace ble_test {

inline bool failAdvertising = false, failRegistration = false, hostRunning = false;
inline int starts = 0, stops = 0, deinits = 0, advertisements = 0, securityRequests = 0, disconnects = 0;
inline ble_gap_conn_desc connection;
inline std::vector<ble_addr_t> bonds;
inline ble_gatt_chr_def characteristic;
inline ble_gatt_chr_def rpcWrite;
inline int (*gapCallback)(ble_gap_event*, void*) = nullptr;
inline void* gapContext = nullptr;
inline std::vector<std::string> uuids;

inline int event(int type, int status = 0) {
    ble_gap_event event{};
    event.type = type;
    event.connect.status = status;
    event.enc_change.status = status;
    return gapCallback(&event, gapContext);
}

inline std::pair<int, std::string> readStatus() {
    os_mbuf buffer;
    ble_gatt_access_ctxt access{&buffer};
    const auto result = characteristic.access_cb(1, 1, &access, characteristic.arg);
    return {result, buffer.value};
}

}  // namespace ble_test

extern bool bleControllerMemoryReserved;

inline int nimble_port_init() {
    assert(bleControllerMemoryReserved);
    ++ble_test::starts;
    return 0;
}

inline int nimble_port_stop() {
    assert(ble_test::hostRunning);
    ble_test::hostRunning = false;
    ++ble_test::stops;
    return 0;
}

inline int nimble_port_deinit() {
    assert(!ble_test::hostRunning);
    ++ble_test::deinits;
    return 0;
}

inline void nimble_port_run() {
}

inline void nimble_port_freertos_deinit() {
}

inline void nimble_port_freertos_init(void (*)(void*)) {
    ble_test::hostRunning = true;
    ble_hs_cfg.sync_cb();
}

inline uint32_t millis() {
    static uint32_t ticks = 0;
    return ++ticks;
}

inline void delay(int) {
}

inline void ble_svc_gap_init() {
}

inline void ble_svc_gatt_init() {
}

inline int ble_svc_gap_device_name_set(const char*) {
    return 0;
}

inline int ble_uuid_from_str(ble_uuid_any_t*, const char* text) {
    ble_test::uuids.emplace_back(text);
    return 0;
}

inline int ble_gatts_count_cfg(ble_gatt_svc_def*) {
    return ble_test::failRegistration ? 1 : 0;
}

inline int ble_gatts_add_svcs(ble_gatt_svc_def* services) {
    ble_test::characteristic = services[0].characteristics[0];
    ble_test::rpcWrite = services[0].characteristics[1];
    if (services[0].characteristics[2].val_handle) *services[0].characteristics[2].val_handle = 77;
    return 0;
}

inline int ble_gap_adv_set_fields(ble_hs_adv_fields* fields) {
    assert(fields->num_uuids128 == 1);
    return 0;
}

inline int ble_gap_adv_rsp_set_fields(ble_hs_adv_fields*) {
    return 0;
}

inline int ble_gap_adv_start(uint8_t, const ble_addr_t*, int, ble_gap_adv_params*,
                             int (*callback)(ble_gap_event*, void*), void* context) {
    ble_test::gapCallback = callback;
    ble_test::gapContext = context;
    ++ble_test::advertisements;
    return ble_test::failAdvertising ? 1 : 0;
}

inline int ble_store_util_bonded_peers(ble_addr_t* peers, int* count, int capacity) {
    assert(ble_test::bonds.size() <= static_cast<std::size_t>(capacity));
    *count = ble_test::bonds.size();
    for (int i = 0; i < *count; ++i) peers[i] = ble_test::bonds[i];
    return 0;
}

inline int ble_addr_cmp(const ble_addr_t* a, const ble_addr_t* b) {
    return a->type == b->type ? std::memcmp(a->val, b->val, 6) : 1;
}

inline int ble_hs_util_ensure_addr(int) {
    return 0;
}

inline int ble_hs_id_infer_auto(int, uint8_t* type) {
    *type = 0;
    return 0;
}

inline int ble_gap_security_initiate(uint16_t) {
    ++ble_test::securityRequests;
    return 0;
}

inline int ble_gap_terminate(uint16_t, int) {
    ++ble_test::disconnects;
    return 0;
}

inline int ble_gap_conn_find(uint16_t, ble_gap_conn_desc* output) {
    *output = ble_test::connection;
    return 0;
}

inline int os_mbuf_append(os_mbuf* output, const void* data, std::size_t length) {
    output->value.append(static_cast<const char*>(data), length);
    return 0;
}

extern "C" inline void ble_store_config_init() {
}

constexpr int BLE_GATT_CHR_F_WRITE = 4, BLE_GATT_CHR_F_WRITE_ENC = 8, BLE_GATT_CHR_F_NOTIFY = 16;
constexpr int BLE_GAP_EVENT_SUBSCRIBE = 5, BLE_ATT_ERR_INSUFFICIENT_AUTHEN = 5, BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN = 13;

struct ble_npl_event {
    void (*callback)(ble_npl_event*) = nullptr;
    void* arg = nullptr;
};

inline void ble_npl_event_init(ble_npl_event* event, void (*callback)(ble_npl_event*), void* arg) {
    *event = {callback, arg};
}

inline void* ble_npl_event_get_arg(ble_npl_event* event) {
    return event->arg;
}

inline void* nimble_port_get_dflt_eventq() {
    return nullptr;
}

inline void ble_npl_eventq_put(void*, ble_npl_event* event) {
    event->callback(event);
}

#define OS_MBUF_PKTLEN(buffer) ((buffer)->value.size())

inline int ble_hs_mbuf_to_flat(os_mbuf* buffer, void* data, uint16_t capacity, uint16_t* length) {
    if (buffer->value.size() > capacity) return 1;
    *length = buffer->value.size();
    std::memcpy(data, buffer->value.data(), *length);
    return 0;
}

inline os_mbuf* ble_hs_mbuf_from_flat(const void* data, uint16_t length) {
    return new os_mbuf{std::string(static_cast<const char*>(data), length)};
}

namespace ble_test {

inline std::string notification;

}

inline int ble_gatts_notify_custom(uint16_t, uint16_t, os_mbuf* buffer) {
    ble_test::notification = buffer->value;
    delete buffer;
    return 0;
}

inline void ble_svc_gatt_changed(uint16_t, uint16_t) {
}
