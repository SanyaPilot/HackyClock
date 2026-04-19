#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "app_manager.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "jsmn.h"

static const char *TAG = "http_api";
static const char status_ok_msg[] = "{\"status\":\"ok\"}";
static const char *json_content_type = "application/json";

static int gen_error_response(char* buf, size_t buf_size, char* msg)
{
    return snprintf(buf, buf_size, "{\"status\":\"err\",\"msg\":\"%s\"}", msg);
}

static int switch_app_handler(httpd_req_t *req)
{
    char buf[128];
    if (req->content_len > (sizeof(buf) - 1))
        return ESP_FAIL;

    int ret;
    while (true) {
        ret = httpd_req_recv(req, buf, req->content_len);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            // Retry on timeout
            continue;
        } else if (ret <= 0) {
            return ESP_FAIL;
        }
        break;
    }

    httpd_resp_set_type(req, json_content_type);

    jsmn_parser parser;
    jsmntok_t tokens[16];
    jsmn_init(&parser);
    ret = jsmn_parse(&parser, buf, req->content_len, tokens, 16);
    if (ret < 0) {
        ESP_LOGW(TAG, "switch_app: Failed to parse JSON! JSMN error %d", ret);
        gen_error_response(buf, sizeof(buf), "invalid_body");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, buf);
        return ESP_OK;
    }
    jsmntok_t *dir_val = NULL;
    for (uint8_t i = 1; i < ret; i++) {
        jsmntok_t *val = &tokens[i];
        jsmntok_t *key = val - 1;
        if (key->type == JSMN_STRING && val->type == JSMN_PRIMITIVE) {
            if (strncmp(&buf[key->start], "dir", key->end - key->start) == 0) {
                dir_val = val;
                break;
            }
        }
    }
    if (dir_val == NULL) {
        ESP_LOGW(TAG, "switch_app: Direction not found");
        gen_error_response(buf, sizeof(buf), "invalid_body");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, buf);
        return ESP_OK;
    }

    // Switch apps
    char dir_sym = buf[dir_val->start];
    if (dir_sym == 't') {
        am_send_msg(AM_MSG_NEXTAPP);
    } else if (dir_sym == 'f') {
        am_send_msg(AM_MSG_PREVAPP);
    } else {
        ESP_LOGW(TAG, "switch_app: Non boolean direction");
        gen_error_response(buf, sizeof(buf), "invalid_body");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, buf);
        return ESP_OK;
    }
    httpd_resp_send(req, status_ok_msg, sizeof(status_ok_msg) - 1);
    return ESP_OK;
}

static int list_apps_handler(httpd_req_t *req)
{
    char resp_buf[25 * CONFIG_HC_AM_MAX_APPS + 32] = "{\"status\":\"ok\",\"apps\":[";
    struct am_app_info* infos = get_apps_list();
    for (uint8_t i = 0; infos[i].name != NULL; i++) {
        char fmt_name[25];
        int wr = snprintf(fmt_name, sizeof(fmt_name), "\"%s\",", infos[i].name);
        if (wr >= sizeof(fmt_name)) {
            ESP_LOGE(TAG, "list_apps: Too small buffer");
            return ESP_FAIL;
        }
        strcat(resp_buf, fmt_name);
    }
    // Truncate last symbol (',')
    resp_buf[strlen(resp_buf)-1] = 0;
    strcat(resp_buf, "]}");
    httpd_resp_set_type(req, json_content_type);
    httpd_resp_sendstr(req, resp_buf);
    return ESP_OK;
}

static const httpd_uri_t handlers[] = {
    { .uri = "/controls/switch_app", .method = HTTP_POST, .handler = switch_app_handler },
    { .uri = "/apps", .method = HTTP_GET, .handler = list_apps_handler },
    { /* sentinel */ }
};

int start_http_server()
{
    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    int ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server! %s", esp_err_to_name(ret));
        return ret;
    }
    // Register handlers here
    for (uint8_t i = 0; handlers[i].uri != NULL; i++) {
        httpd_register_uri_handler(server, &handlers[i]);
    }
    return ESP_OK;
}
