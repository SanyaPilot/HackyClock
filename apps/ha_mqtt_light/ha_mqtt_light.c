#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"

#include "app_manager.h"
#include "animations.h"
#include "canvas.h"
#include "framebuffer.h"
#include "mqtt.h"

#define MQTT_DISCOVERY_TOPIC    CONFIG_HA_LIGHT_DISCOVERY_PREFIX "/device/%s/config"

#define UI_MSG_TOGGLE   0x8
#define UI_MSG_UPDATE   0x10

#define FADE_ANIM_DURATION  300
#define FADE_ANIM_DELAY     20

enum mqtt_topic_type {
    STATUS_TOPIC_MAIN = 0,
    STATUS_TOPIC_BRIGHTNESS,
    STATUS_TOPIC_RGB,

    COMMAND_TOPIC_MAIN,
    COMMAND_TOPIC_BRIGHTNESS,
    COMMAND_TOPIC_RGB
};
#define MQTT_TOPIC_COUNT  COMMAND_TOPIC_RGB + 1
static const char *mqtt_topics_postfixes[] = {
    "/led_matrix/status",
    "/led_matrix/brightness/status",
    "/led_matrix/rgb/status",

    "/led_matrix/switch",
    "/led_matrix/brightness/set",
    "/led_matrix/rgb/set"
};
static const char *mqtt_topics_keys[] = {
    "stat_t",
    "bri_stat_t",
    "rgb_stat_t",

    "cmd_t",
    "bri_cmd_t",
    "rgb_cmd_t"
};
static char *mqtt_topics[MQTT_TOPIC_COUNT];

static const char *discovery_template = "{"
    "\"dev\":{"
        "\"ids\":\"%1$s\","  // Device ID
        "\"name\":\"" CONFIG_HA_LIGHT_DEVICE_NAME "\","
        "\"mf\":\"" CONFIG_HA_LIGHT_DEVICE_MANUFACTURER "\","
        "\"sn\":\"%1$s\""
    "},"
    "\"o\":{\"name\":\"" CONFIG_HA_LIGHT_ORIGIN_NAME "\"},"
    "\"cmps\":{"
        "\"led_matrix_light\":{"
            "\"p\":\"light\","
            "\"name\":\"" CONFIG_HA_LIGHT_OBJECT_NAME "\","
            "%2$s"  // MQTT topic definitions
            "\"uniq_id\": \"%1$s_ledmatrix\""
        "}"
    "},"
    "\"qos\": 2"
"}";

static const char *TAG = "ha_mqtt_light_app";

extern bool wifi_is_connected;

static char device_id[6 * 2 + 3 + 1];  // 6 bytes in HEX (ignore last 2 if MAC is 64 bit) in hex + "hc_"

static struct {
    bool state;
    uint8_t brightness;
    crgb color;
} cur_params = { false, 255, { 255, 255, 255 } };

static TaskHandle_t ui_task_handle;

static int8_t publish_main_state(struct mqtt_client *client)
{
    if (client == NULL)
        return ESP_FAIL;

    int8_t ret = mqtt_publish(client, mqtt_topics[STATUS_TOPIC_MAIN], cur_params.state ? "ON" : "OFF", 0, 0);
    if (ret < 0)
        ESP_LOGE(TAG, "Failed to publish current state! Ret: %d", ret);
    return ret;
}

static int8_t publish_brightness_state(struct mqtt_client *client)
{
    if (client == NULL)
        return ESP_FAIL;

    char br_str[4];
    itoa(cur_params.brightness, br_str, 10);
    int8_t ret = mqtt_publish(client, mqtt_topics[STATUS_TOPIC_BRIGHTNESS], br_str, 0, 0);
    if (ret < 0)
        ESP_LOGE(TAG, "Failed to publish current brightness state! Ret: %d", ret);
    return ret;
}

static int8_t publish_rgb_state(struct mqtt_client *client)
{
    if (client == NULL)
        return ESP_FAIL;

    char rgb_str[3 * 3 + 3];
    sprintf(rgb_str, "%d,%d,%d", cur_params.color.r, cur_params.color.g, cur_params.color.b);
    int8_t ret = mqtt_publish(client, mqtt_topics[STATUS_TOPIC_RGB], rgb_str, 0, 0);
    if (ret < 0)
        ESP_LOGE(TAG, "Failed to publish current RGB state! Ret: %d", ret);
    return ret;
}

static void switch_handler(struct mqtt_client *client, char *topic, char *data)
{
    ESP_LOGI(TAG, "Received data: %s", data);
    bool state = (strcmp(data, "ON") == 0) ? true : false;
    if (state != cur_params.state) {
        cur_params.state = state;
        xTaskNotify(ui_task_handle, UI_MSG_TOGGLE, eSetBits);
    }
    publish_main_state(client);
}

static void brightness_handler(struct mqtt_client *client, char *topic, char *data)
{
    ESP_LOGI(TAG, "Received data: %s", data);
    uint8_t brightness = atoi(data);
    if (brightness != cur_params.brightness) {
        cur_params.brightness = brightness;
        xTaskNotify(ui_task_handle, UI_MSG_UPDATE, eSetBits);
    }
    mqtt_publish(client, mqtt_topics[STATUS_TOPIC_BRIGHTNESS], data, 0, 0);
}

static void rgb_handler(struct mqtt_client *client, char *topic, char *data)
{
    ESP_LOGI(TAG, "Received data: %s", data);
    char data_bak[3 * 3 + 3];
    strcpy(data_bak, data);

    char *token;
    char *saveptr;
    uint8_t color[3];
    uint8_t i = 0;
    for (token = strtok_r(data, ",", &saveptr); token != NULL && i < 3; token = strtok_r(NULL, ",", &saveptr)) {
        color[i] = atoi(token);
        i++;
    }
    if (i < 3) {
        ESP_LOGE(TAG, "Failed to parse RGB data!");
        return;
    }
    if (color[0] != cur_params.color.r || color[1] != cur_params.color.g || color[2] != cur_params.color.b) {
        crgb crgb_color = { color[0], color[1], color[2] };
        cur_params.color = crgb_color;
        xTaskNotify(ui_task_handle, UI_MSG_UPDATE, eSetBits);
    }
    mqtt_publish(client, mqtt_topics[STATUS_TOPIC_RGB], data_bak, 0, 0);
}

static const struct {
    const enum mqtt_topic_type topic;
    mqtt_handler_t handler;
} handlers[] = {
    { COMMAND_TOPIC_MAIN, switch_handler },
    { COMMAND_TOPIC_BRIGHTNESS, brightness_handler },
    { COMMAND_TOPIC_RGB, rgb_handler },
    { /* sentinel */ }
};

static void on_mqtt_connected(struct mqtt_client *client)
{
    // Subscribe to topics
    for (uint8_t i = 0; (void *)(handlers[i].topic) != NULL; i++) {
        mqtt_subscribe(client, mqtt_topics[handlers[i].topic], 0, handlers[i].handler);
    }

    // Publish discover payload
    char topic[128];
    char payload[1024];  // 1 KiB should be fine
    char topic_definitions[512] = "";
    for (uint8_t i = 0; i < MQTT_TOPIC_COUNT; i++) {
        char buf[128];
        sprintf(buf, "\"%s\":\"%s\",", mqtt_topics_keys[i], mqtt_topics[i]);
        strcat(topic_definitions, buf);
    }
    sprintf(topic, MQTT_DISCOVERY_TOPIC, device_id);
    sprintf(payload, discovery_template, device_id, topic_definitions);
    ESP_LOGI(TAG, " %s", payload);
    int ret = mqtt_publish(client, topic, payload, 2, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to publish discover payload! Ret: %d", ret);
    }

    // Publish current state
    publish_main_state(client);
    publish_brightness_state(client);
    publish_rgb_state(client);
}

static struct mqtt_client *init_mqtt(void)
{
    // Get MAC address to use as an ID
    uint8_t mac[esp_mac_addr_len_get(ESP_MAC_EFUSE_FACTORY)];
    esp_efuse_mac_get_default(mac);
    sprintf(device_id, "hc_%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Prepare topics (concat with device ID)
    char topic[128];
    for (uint8_t i = 0; i < MQTT_TOPIC_COUNT; i++) {
        strcpy(topic, device_id);
        strcat(topic, mqtt_topics_postfixes[i]);
        mqtt_topics[i] = pvPortMalloc(strlen(topic) + 1);
        strcpy(mqtt_topics[i], topic);
    }

    while (!wifi_is_connected) {
        ESP_LOGI(TAG, "Waiting for Wi-Fi to connect...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    struct mqtt_client *client = mqtt_init(CONFIG_HA_LIGHT_MQTT_BROKER_URL);
    if (mqtt_connect(client, on_mqtt_connected) != ESP_OK)
        return NULL;
    return client;
}

void ha_mqtt_light_ui_task(void *param)
{
    struct canvas *cv = (struct canvas *)param;

    ui_task_handle = xTaskGetCurrentTaskHandle();

    struct mqtt_client *client = init_mqtt();
    uint32_t message;
    while (1) {
        if (xTaskNotifyWait(pdFALSE, ULONG_MAX, &message, portMAX_DELAY) == pdFALSE)
            continue;

        if ((message & UI_MSG_TOGGLE) || (message & EVENT_BTN_CLICK)) {
            // Toggle light with fading animation
            if (message == EVENT_BTN_CLICK) {
                cur_params.state = !cur_params.state;
                publish_main_state(client);
            }
            if (cur_params.state) {
                struct canvas *temp_cv = cv_copy(cv);
                cv_fill(temp_cv, crgb_mult(cur_params.color, cur_params.brightness / 255.f));
                anim_fade_in(cv, temp_cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
                cv_free(temp_cv);
            } else {
                anim_fade_out(cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
            }
        } else if (message & UI_MSG_UPDATE) {
            if (cur_params.state) {
                cv_fill(cv, crgb_mult(cur_params.color, cur_params.brightness / 255.f));
            } else {
                cv_blank(cv);
            }
            am_send_msg(AM_MSG_REFRESH);
        } else if (message & EVENT_KNOB_RIGHT) {
            if (cur_params.brightness < 255) {
                cur_params.brightness = MIN(cur_params.brightness + CONFIG_HA_LIGHT_BRIGHTNESS_STEP, 255);
                publish_brightness_state(client);
                cv_fill(cv, crgb_mult(cur_params.color, cur_params.brightness / 255.f));
                am_send_msg(AM_MSG_REFRESH);
            }
        } else if (message & EVENT_KNOB_LEFT) {
            if (cur_params.brightness > 0) {
                cur_params.brightness = MAX(cur_params.brightness - CONFIG_HA_LIGHT_BRIGHTNESS_STEP, 0);
                publish_brightness_state(client);
                cv_fill(cv, crgb_mult(cur_params.color, cur_params.brightness / 255.f));
                am_send_msg(AM_MSG_REFRESH);
            }
        }
    }
}
