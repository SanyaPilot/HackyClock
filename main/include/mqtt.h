#ifndef __MQTT_H__
#define __MQTT_H__

#include <stdbool.h>
#include <stdint.h>
#include "mqtt_client.h"

#define MQTT_MAX_HANDLERS   32

struct mqtt_client;
typedef void (*mqtt_handler_t)(struct mqtt_client *client, char *topic, char *data);
typedef void (*mqtt_lifecycle_handler_t)(struct mqtt_client *client);

struct mqtt_handler_desc {
    const char *topic;
    mqtt_handler_t handler;
};

struct mqtt_client {
    const char *url;
    bool is_connected;
    struct mqtt_handler_desc handlers[MQTT_MAX_HANDLERS];
    uint8_t handlers_count;
    esp_mqtt_client_handle_t esp_client;
    mqtt_lifecycle_handler_t on_connected_handler;
};

struct mqtt_client *mqtt_init(const char *url);
int mqtt_connect(struct mqtt_client *client, mqtt_lifecycle_handler_t on_connected);
int mqtt_subscribe(struct mqtt_client *client, const char *topic, uint8_t qos, mqtt_handler_t handler);
int mqtt_unsubscribe(struct mqtt_client *client, const char *topic);
int mqtt_publish(struct mqtt_client *client, const char *topic, const char *data, uint8_t qos, uint8_t retain);

#endif
