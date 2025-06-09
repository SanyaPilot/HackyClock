#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "canvas.h"
#include "image_utils.h"
#include "app_manager.h"

extern bool wifi_is_connected;
extern const char *base_path;
static const char *wifi_path_base = "/wifi/";

#define RSSI_LEVEL_COUNT    4
static const int8_t rssi_thresholds[] = { -70, -60, -50};
static const char *rssi_icon_files[] = {
    "empty.qoi",
    "level1.qoi",
    "level2.qoi",
    "level3.qoi",
    "level4.qoi"
};

#define RSSI_CHECK_DELAY    2000

static const char *TAG = "wifi_status";

static void draw_icon_centered(struct canvas *cv, struct image_desc *icon)
{
    uint8_t x = (cv->width - icon->width) / 2;
    uint8_t y = (cv->height - icon->height) / 2;
    cv_draw_image(cv, icon, x, y);
    am_send_msg(AM_MSG_REFRESH);
}

void wifi_status_ui_task(void *param)
{
    struct canvas *cv = (struct canvas *)param;

    // Preload all images
    struct image_desc rssi_icons[RSSI_LEVEL_COUNT + 1];  // +1 for empty image
    struct image_desc img_desc;
    char path[64];
    for (uint8_t i = 0; i < RSSI_LEVEL_COUNT + 1; i++) {
        strcpy(path, base_path);
        strcat(path, wifi_path_base);
        strcat(path, rssi_icon_files[i]);
        uint8_t ret = load_image_file(path, &img_desc);
        if (ret)
            while (1) { vTaskSuspend(NULL); }  // Halt task

        rssi_icons[i] = img_desc;
    }

    int rssi;
    while (1) {
        if (wifi_is_connected) {
            esp_err_t ret = esp_wifi_sta_get_rssi(&rssi);
            if (ret == ESP_OK) {
                uint8_t level = 0;
                while(level < (RSSI_LEVEL_COUNT - 1) && rssi > rssi_thresholds[level])
                    level++;

                ESP_LOGI(TAG, "RSSI: %d, level: %d", rssi, level);
                draw_icon_centered(cv, &rssi_icons[level + 1]);
            } else
                ESP_LOGW(TAG, "Failed to get RSSI!");
        } else {
            draw_icon_centered(cv, &rssi_icons[0]);
        }
        vTaskDelay(pdMS_TO_TICKS(RSSI_CHECK_DELAY));
    }
}
