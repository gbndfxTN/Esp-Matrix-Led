#include "OtaCore.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "BLE_OTA";

#define OTA_SERVICE_UUID        0x00FF
#define OTA_CHAR_RX_UUID        0xFF01
#define OTA_CHAR_TX_UUID        0xFF02
#define DEVICE_NAME             "MatrixLed-OTA"
#define GATTS_MAX_ATTR_NUM      6

enum {
    IDX_SVC,
    IDX_CHAR_RX_DECL,
    IDX_CHAR_RX_VAL,
    IDX_CHAR_TX_DECL,
    IDX_CHAR_TX_VAL,
};

static uint8_t srv_inst;
static uint16_t conn_id;
static int gatt_if;
static bool ota_active = false;

static esp_ota_handle_t ota_handle;
static const esp_partition_t *ota_part;
static uint32_t ota_total;
static uint32_t ota_written;
static bool header_received = false;

static uint8_t char_prop_write  = ESP_GATT_CHAR_PROP_BIT_WRITE;
static uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const esp_gatts_attr_db_t gatt_db[GATTS_MAX_ATTR_NUM] = {
    [IDX_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_PRI_SERVICE}, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&(uint16_t){OTA_SERVICE_UUID}},
    },
    [IDX_CHAR_RX_DECL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_write},
    },
    [IDX_CHAR_RX_VAL] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){OTA_CHAR_RX_UUID}, ESP_GATT_PERM_WRITE,
         512, 0, NULL},
    },
    [IDX_CHAR_TX_DECL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){ESP_GATT_UUID_CHAR_DECLARE}, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_notify},
    },
    [IDX_CHAR_TX_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&(uint16_t){OTA_CHAR_TX_UUID}, ESP_GATT_PERM_READ,
         20, 0, NULL},
    },
};

static void ota_begin(uint32_t total_size) {
    ota_part = esp_ota_get_next_update_partition(NULL);
    if (!ota_part) {
        ESP_LOGE(TAG, "Aucune partition OTA trouvee");
        return;
    }
    esp_err_t err = esp_ota_begin(ota_part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin echoue: %s", esp_err_to_name(err));
        return;
    }
    ota_total = total_size;
    ota_written = 0;
    header_received = true;
    ESP_LOGI(TAG, "OTA commence: %" PRIu32 " octets sur %s",
             total_size, ota_part->label);
}

static void ota_write_chunk(const uint8_t *data, uint16_t len) {
    if (!header_received) {
        if (len < 4) return;
        uint32_t total_size;
        memcpy(&total_size, data, 4);
        ota_begin(total_size);
        return;
    }
    esp_err_t err = esp_ota_write(ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write echoue: %s", esp_err_to_name(err));
        return;
    }
    ota_written += len;
    ESP_LOGI(TAG, "OTA: %" PRIu32 " / %" PRIu32, ota_written, ota_total);
}

static void ota_finish(void) {
    if (!header_received) return;
    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end echoue: %s", esp_err_to_name(err));
        return;
    }
    err = esp_ota_set_boot_partition(ota_part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition echoue: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "OTA termine, redemarrage...");
    esp_restart();
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
            .adv_int_min        = 0x20,
            .adv_int_max        = 0x40,
            .adv_type           = ADV_TYPE_IND,
            .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
            .channel_map        = ADV_CHNL_ALL,
            .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        });
        break;
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t iface,
                                esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        gatt_if = iface;
        esp_ble_gap_set_device_name(DEVICE_NAME);
        esp_ble_gap_config_adv_data_raw((uint8_t[]){
            0x02, 0x01, 0x06,
            0x0F, 0x09, 'M','a','t','r','i','x','L','e','d','-','O','T','A',
        }, 18);
        esp_ble_gatts_create_attr_tab(gatt_db, gatt_if, GATTS_MAX_ATTR_NUM, srv_inst);
        break;
    case ESP_GATTS_CREATE_EVT:
        esp_ble_gatts_start_service(param->create.service_handle);
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "Service BLE OTA demarre");
        break;
    case ESP_GATTS_CONNECT_EVT:
        conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "Client connecte (conn_id=%d)", conn_id);
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, 6);
        conn_params.min_int = 0x10;
        conn_params.max_int = 0x20;
        conn_params.latency = 0;
        conn_params.timeout = 400;
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        conn_id = 0;
        ota_active = false;
        ESP_LOGI(TAG, "Client deconnecte");
        esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
            .adv_int_min        = 0x20,
            .adv_int_max        = 0x40,
            .adv_type           = ADV_TYPE_IND,
            .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
            .channel_map        = ADV_CHNL_ALL,
            .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        });
        break;
    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == gatt_db[IDX_CHAR_RX_VAL].att_desc.handle) {
            if (!header_received && param->write.len >= 4) {
                ota_write_chunk(param->write.value, param->write.len);
            } else if (header_received) {
                ota_write_chunk(param->write.value, param->write.len);
                if (ota_written >= ota_total) {
                    ota_finish();
                }
            }
        }
        if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatt_if, param->write.conn_id,
                                        param->write.trans_id, ESP_GATT_OK, NULL);
        }
        break;
    default:
        break;
    }
}

void ble_ota_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(0);

    ota_active = true;
    header_received = false;
    ESP_LOGI(TAG, "BLE OTA initialise");
}

int ble_ota_is_active(void) {
    return ota_active;
}
