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
#define HTTP_DEFAULT_CHUNKED_BUFFER_SIZE    4096

static const char *TAG = "http_client";

static int do_request(const char *url, esp_http_client_method_t type, struct http_header *headers, const char *body, char **buffer, size_t chunked_buf_size)
{
    esp_http_client_config_t config = {
        .url = url,
        .method = type,
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (headers != NULL) {
        for (uint8_t i = 0; (void *)(headers[i].key) != NULL; i++)
            esp_http_client_set_header(client, headers[i].key, headers[i].value);
    }
    size_t write_len = 0;
    if ((type == HTTP_METHOD_POST || type == HTTP_METHOD_PUT) && body != NULL)
        write_len = strlen(body);

    for (uint8_t i = 0; i < HTTP_RETRY_COUNT; i++) {
        // Open connection
        esp_err_t err;
        if ((err = esp_http_client_open(client, write_len)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            continue;
        }
        if (write_len > 0) {
            if (esp_http_client_write(client, body, write_len) < 0) {
                ESP_LOGE(TAG, "Failed to write body");
                esp_http_client_close(client);
                continue;
            }
        }
        int content_len = esp_http_client_fetch_headers(client);
        if (content_len < 0) {
            ESP_LOGW(TAG, "Timeout or some other error while fetching headers");
            esp_http_client_close(client);
            continue;
        }

        // Buffer allocation
        size_t buf_size;
        if (!esp_http_client_is_chunked_response(client)) {
            // Allocate buffer with Content-Length size
            buf_size = content_len + 1;
        } else {
            // Allocate buffer with predefined size
            buf_size = chunked_buf_size;
        }
        *buffer = pvPortMalloc(buf_size);
        if (*buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for an output buffer");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        memset(*buffer, 0, buf_size);

        // Read response
        size_t read_len = 0;
        while (read_len < buf_size) {
            int chunk_len = esp_http_client_read(client, *buffer + read_len, buf_size - read_len);
            if (chunk_len < 0) {
                ESP_LOGW(TAG, "Timeout or some other error while fetching data");
                esp_http_client_close(client);
                continue;
            }
            if (chunk_len == 0)
                break;
            read_len += chunk_len;
        }

        int code = esp_http_client_get_status_code(client);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return code;
    }
    return ESP_FAIL;
}

int http_get(const char *url, struct http_header *headers, char **response)
{
    return do_request(url, HTTP_METHOD_GET, headers, NULL, response, HTTP_DEFAULT_CHUNKED_BUFFER_SIZE);
}

int http_get_with_bufsize(const char *url, struct http_header *headers, char **response, size_t buf_size)
{
    return do_request(url, HTTP_METHOD_GET, headers, NULL, response, buf_size);
}

int http_post(const char *url, struct http_header *headers, const char *body, char **response)
{
    return do_request(url, HTTP_METHOD_POST, headers, body, response, HTTP_DEFAULT_CHUNKED_BUFFER_SIZE);
}

int http_post_with_bufsize(const char *url, struct http_header *headers, const char *body, char **response, size_t buf_size)
{
    return do_request(url, HTTP_METHOD_POST, headers, body, response, buf_size);
}

int http_post_json(const char *url, const char *body, char **response)
{
    struct http_header headers[] = {
        { .key = "Content-Type", .value = "application/json" },
        { /* sentinel */ }
    };
    return http_post(url, headers, body, response);
}
