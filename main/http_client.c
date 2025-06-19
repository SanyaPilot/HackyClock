#include <string.h>
#include <stdint.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_http_client.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "http_client.h"

#define HTTP_RETRY_COUNT    3

static const char *TAG = "http_client";

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static int bytes_read;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA");
            if (evt->user_data == NULL) {
                ESP_LOGE(TAG, "Output buffer must be specified!");
                return ESP_FAIL;
            }

            // Ignore chunked responses
            if (esp_http_client_is_chunked_response(evt->client))
                break;

            char **buffer = (char **) evt->user_data;
            size_t content_len = esp_http_client_get_content_length(evt->client);

            // Clean the buffer in case of a new request
            if (bytes_read == 0) {
                *buffer = pvPortMalloc(content_len + 1);
                if (*buffer == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for an output buffer");
                    return ESP_FAIL;
                }
                memset(*buffer, 0, content_len + 1);
            }

            size_t copy_len = MIN(evt->data_len, content_len - bytes_read);
            if (copy_len) {
                memcpy(*buffer + bytes_read, evt->data, copy_len);
            }
            bytes_read += copy_len;
            break;
        
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            bytes_read = 0;
            break;
        
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGW(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGW(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            bytes_read = 0;
            break;
        
        default:
            break;
    }
    return ESP_OK;
}

static esp_http_client_handle_t prepare_http_client(const char *url, struct http_header *headers, char **response)
{
    *response = NULL;
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .user_data = response,
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (headers != NULL) {
        for (uint8_t i = 0; (void *)(headers[i].key) != NULL; i++) {
            esp_http_client_set_header(client, headers[i].key, headers[i].value);
        }
    }
    return client;
}

static int try_request(esp_http_client_handle_t client, const char *type)
{
    for (uint8_t i = 0; i < HTTP_RETRY_COUNT; i++) {
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            return esp_http_client_get_status_code(client);
        } else {
            ESP_LOGW(TAG, "HTTP %s request failed: %s", type, esp_err_to_name(err));
        }
    }
    return -1;
}

int http_get(const char *url, struct http_header *headers, char **response)
{
    esp_http_client_handle_t client = prepare_http_client(url, headers, response);
    int ret = try_request(client, "GET");
    esp_http_client_cleanup(client);
    return ret;
}

int http_post(const char *url, struct http_header *headers, const char *body, char **response)
{
    esp_http_client_handle_t client = prepare_http_client(url, headers, response);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, body, strlen(body));

    int ret = try_request(client, "POST");
    esp_http_client_cleanup(client);
    return ret;
}

int http_post_json(const char *url, const char *body, char **response)
{
    struct http_header headers[] = {
        { .key = "Content-Type", .value = "application/json" },
        { /* sentinel */ }
    };
    return http_post(url, headers, body, response);
}
