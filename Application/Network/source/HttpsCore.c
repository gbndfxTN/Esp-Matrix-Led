#include "HttpsCore.h"

static const char *TAG = "HTTPS_PSRAM";

uint8_t* download_gif(const char *url, size_t *out_size) {
    *out_size = 0;

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Échec d'ouverture de la connexion HTTPS");
        esp_http_client_cleanup(client);
        return NULL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Taille du fichier inconnue dans la réponse HTTP");
        esp_http_client_cleanup(client);
        return NULL;
    }

    uint8_t *psram_buffer = (uint8_t *)heap_caps_malloc(content_length, MALLOC_CAP_SPIRAM);
    if (psram_buffer == NULL) {
        ESP_LOGE(TAG, "Mémoire PSRAM insuffisante pour %d octets", content_length);
        esp_http_client_cleanup(client);
        return NULL;
    }

    int total_read = 0;
    int read_len = 0;

    while (total_read < content_length) {
        read_len = esp_http_client_read(client, (char *)psram_buffer + total_read, content_length - total_read);
        if (read_len <= 0) break;
        total_read += read_len;
    }

    esp_http_client_cleanup(client);

    if (total_read != content_length) {
        ESP_LOGE(TAG, "Téléchargement interrompu (%d/%d octets lus)", total_read, content_length);
        heap_caps_free(psram_buffer);
        return NULL;
    }

    *out_size = total_read;
    ESP_LOGI(TAG, "GIF téléchargé avec succès en PSRAM (%d octets)", total_read);
    return psram_buffer;
}