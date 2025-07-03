#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "mqtt_client.h"
#include "mqtt.h"

static const char *TAG = "mqtt";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    struct mqtt_client *client = handler_args;
    static char *msg_buffer = NULL;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
        client->is_connected = true;
        if (client->on_connected_handler != NULL)
            client->on_connected_handler(client);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
        client->is_connected = false;
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "MQTT_EVENT_DATA");
        // Call appropriate handler
        struct mqtt_handler_desc *desc = NULL;
        char topic[128];
        uint8_t topic_len = MIN(event->topic_len, 127);
        strncpy(topic, event->topic, topic_len);
        topic[topic_len] = 0;
        for (uint8_t i = 0; i < client->handlers_count; i++) {
            ESP_LOGI(TAG, "%s", client->handlers[i].topic);
            if (strcmp(client->handlers[i].topic, topic) == 0) {
                desc = &client->handlers[i];
                break;
            }
        }
        if (desc == NULL) {
            ESP_LOGD(TAG, "No handler found for topic %s", topic);
            break;
        }
        if (event->data_len == event->total_data_len && msg_buffer == NULL) {
            // Just call handler
            msg_buffer = pvPortMalloc(event->data_len + 1);
            strncpy(msg_buffer, event->data, event->data_len);
            msg_buffer[event->data_len] = 0;
            desc->handler(client, topic, msg_buffer);
            free(msg_buffer);
            msg_buffer = NULL;
        } else if (event->current_data_offset == 0 && msg_buffer == NULL) {
            // Allocate buffer for all message
            msg_buffer = pvPortMalloc(event->total_data_len + 1);
            if (msg_buffer == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for an output buffer");
                break;
            }
            memcpy(msg_buffer, event->data, event->data_len);
        } else if (msg_buffer != NULL) {
            // Continue copying message
            memcpy(msg_buffer + event->current_data_offset, event->data, event->data_len);
            if (event->current_data_offset + event->data_len == event->total_data_len) {
                // Message finished, call handler
                msg_buffer[event->total_data_len] = 0;
                desc->handler(client, event->topic, event->data);
                free(msg_buffer);
                msg_buffer = NULL;
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGD(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGE(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGE(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                    strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGE(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGD(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

struct mqtt_client *mqtt_init(const char *url)
{
    struct mqtt_client *client = pvPortMalloc(sizeof(struct mqtt_client));
    if (client != NULL) {
        client->url = url;
        client->is_connected = 0;
        client->handlers_count = 0;
        client->esp_client = NULL;
        client->on_connected_handler = NULL;
    }
    return client;
}

int mqtt_connect(struct mqtt_client *client, mqtt_lifecycle_handler_t on_connected)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = client->url,
    };

    esp_mqtt_client_handle_t esp_client = esp_mqtt_client_init(&mqtt_cfg);
    client->esp_client = esp_client;
    client->on_connected_handler = on_connected;
    esp_mqtt_client_register_event(esp_client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    return esp_mqtt_client_start(esp_client);
}

int mqtt_subscribe(struct mqtt_client *client, const char *topic, uint8_t qos, mqtt_handler_t handler)
{
    if (!client->is_connected) {
        ESP_LOGE(TAG, "Client is disconnected");
        return ESP_FAIL;
    }
    if (client->handlers_count == MQTT_MAX_HANDLERS) {
        ESP_LOGE(TAG, "Reached maximum number of handlers");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "sub: %d, %s", client->handlers_count, topic);
    char *topic_on_heap = pvPortMalloc(strlen(topic) + 1);
    strcpy(topic_on_heap, topic);
    struct mqtt_handler_desc desc = { topic_on_heap, handler };
    client->handlers[client->handlers_count] = desc;
    client->handlers_count++;

    return esp_mqtt_client_subscribe(client->esp_client, topic, qos);
}

int mqtt_unsubscribe(struct mqtt_client *client, const char *topic)
{
    if (!client->is_connected) {
        ESP_LOGE(TAG, "Client is disconnected");
        return ESP_FAIL;
    }
    uint8_t i = 0;
    for (; i < client->handlers_count; i++) {
        if (strcmp(client->handlers[i].topic, topic) == 0)
            break;
    }
    if (i == client->handlers_count) {
        ESP_LOGE(TAG, "Tried to unsubscribe non-existent topic %s", topic);
        return ESP_FAIL;
    }
    int ret = esp_mqtt_client_unsubscribe(client->esp_client, topic);
    free((void *)client->handlers[i].topic);
    for (uint8_t j = i; j < client->handlers_count - 1; j++)
        client->handlers[j] = client->handlers[j + 1];
    client->handlers_count--;
    return ret;
}

int mqtt_publish(struct mqtt_client *client, const char *topic, const char *data, uint8_t qos, uint8_t retain)
{
    return esp_mqtt_client_publish(client->esp_client, topic, data, strlen(data), qos, retain);
}
