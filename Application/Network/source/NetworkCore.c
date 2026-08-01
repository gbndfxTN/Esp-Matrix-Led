#include "NetworkCore.h"
#include "secrets.h"
#include "config.h"

#include <time.h>
#include <sys/time.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_sntp.h>
#include <esp_log.h>
#include <freertos/event_groups.h>

static const char *TAG = "WIFI";

static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int NTP_SYNCED_BIT = BIT1;
static bool sntp_started = false;

void wifi_wait_connected(TickType_t timeout) {
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, timeout);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi deconnecte, reconnexion...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        wifi_ap_record_t ap;
        int rssi = 0;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;
        ESP_LOGI(TAG, "WiFi connecte IP=" IPSTR " RSSI=%d",
                 IP2STR(&event->ip_info.ip), rssi);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void sntp_sync_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Heure NTP synchronisee");
    xEventGroupSetBits(wifi_event_group, NTP_SYNCED_BIT);
}

static void initialize_sntp(void) {
    if (sntp_started) return;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();
    sntp_started = true;
}

void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();

    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi connexion a '%s'...", WIFI_SSID);
}

void wifi_wait_synced(TickType_t wifi_timeout, TickType_t ntp_timeout) {
    wifi_wait_connected(wifi_timeout);
    if (time(NULL) >= 100000) {
        return;
    }
    if (!sntp_started) {
        initialize_sntp();
    }
    xEventGroupWaitBits(wifi_event_group, NTP_SYNCED_BIT, false, true, ntp_timeout);
}

void wifi_wait_for_connection(void) {
    wifi_wait_synced(pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS),
                     pdMS_TO_TICKS(NTP_TIMEOUT_MS));
}
