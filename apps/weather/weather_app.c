#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "canvas.h"
#include "fonts.h"
#include "app_manager.h"
#include "animations.h"
#include "http_client.h"

#include "image_utils.h"
#include "jsmn.h"

#define API_FETCH_DELAY         (CONFIG_WEATHER_FETCH_DELAY * 1000)
#define API_FETCH_FAIL_DELAY    (CONFIG_WEATHER_FETCH_RETRY_DELAY * 1000)

#define JSON_TOKEN_COUNT    64

#define UI_MSG_UPDATE   0x1

#define FADE_ANIM_DURATION  500
#define FADE_ANIM_DELAY     20

static const char *TAG = "weather_app";

enum weather_status {
    WEATHER_SUNNY = 0,
    WEATHER_PARTLY_CLOUDLY,
    WEATHER_CLOUDLY,
    WEATHER_LIGHT_RAIN,
    WEATHER_HEAVY_RAIN,
    WEATHER_SNOWY,
    WEATHER_THUNDERSTORM
};
extern const char *base_path;
static const char *weather_path = "/weather/";
static const char *icon_files[] = {
    "sunny.qoi",
    "partly_cloudly.qoi",
    "cloudly.qoi",
    "light_rain.qoi",
    "heavy_rain.qoi",
    "snowy.qoi",
    "thunderstorm.qoi"
};

#define ICON_COUNT  (WEATHER_THUNDERSTORM + 1)

static const char *url = "https://api.open-meteo.com/v1/forecast"
                         "?latitude=" CONFIG_WEATHER_LAT
                         "&longitude=" CONFIG_WEATHER_LON
                         "&current=temperature_2m,relative_humidity_2m,weather_code";

static struct weather_info {
    enum weather_status status;
    int8_t temperature;
} cur_weather;

static TaskHandle_t ui_task_handle;

static const crgb color = {255, 255, 255};

static int8_t fetch_api(struct weather_info* info)
{
    ESP_LOGI(TAG, "Fetching API...");
    char *response = NULL;
    int code = http_get(url, NULL, &response);
    
    if (code != 200 || response == NULL) {
        ESP_LOGE(TAG, "API returned non-200 code or response is NULL");
        ESP_LOGE(TAG, "Code: %d, Response: %s", code, (response == NULL) ? "<NULL>" : response);
        return -1;
    }

    ESP_LOGD(TAG, "Response: %s", response);
    // Parse JSON
    jsmn_parser parser;
    jsmntok_t tokens[JSON_TOKEN_COUNT];
    jsmn_init(&parser);
    int ret = jsmn_parse(&parser, response, strlen(response), tokens, JSON_TOKEN_COUNT);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to parse JSON! JSMN error %d", ret);
        return -1;
    }
    // Find "current" object
    jsmntok_t *cur_obj = NULL;
    uint8_t cur_obj_pos = 0;
    for (uint8_t i = 1; i < ret; i++) {
        jsmntok_t *obj = &tokens[i];
        jsmntok_t *key = obj - 1;
        if (key->type == JSMN_STRING && obj->type == JSMN_OBJECT) {
            // Check key, and move to the next non-object if doesn't match
            if (strncmp(&response[key->start], "current", key->end - key->start) == 0) {
                cur_obj = obj;
                cur_obj_pos = i;
                break;
            } else {
                i += obj->size;
            }
        }
    }
    if (cur_obj == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON! No current weather data");
        return -1;
    }
    // Find temperature and weather code
    bool temp_found = false;
    float temp_f = 0;
    int8_t w_code = -1;
    for (uint8_t i = cur_obj_pos + 1; i < ret; i += 2) {
        jsmntok_t *key = &tokens[i];
        jsmntok_t *val = key + 1;
        if (key->type == JSMN_STRING && val->type == JSMN_PRIMITIVE) {
            char buf[8];
            uint8_t val_size = MIN(val->end - val->start, 7);
            if (strncmp(&response[key->start], "temperature_2m", key->end - key->start) == 0) {
                strncpy(buf, &response[val->start], val_size);
                buf[val_size] = 0;
                temp_f = atoff(buf);
                temp_found = true;
            } else if (strncmp(&response[key->start], "weather_code", key->end - key->start) == 0) {
                strncpy(buf, &response[val->start], val_size);
                buf[val_size] = 0;
                w_code = atoi(buf);
            }
        }
        if (temp_found && w_code >= 0)
            break;
    }
    free(response);
    if (!(temp_found && w_code >= 0)) {
        ESP_LOGE(TAG, "Failed to parse JSON! No temperature or weather_code!");
        return -1;
    }
    
    info->temperature = (int8_t) round(temp_f);
    // Choose weather status (WMO Weather interpretation codes)
    if (w_code <= 1)
        info->status = WEATHER_SUNNY;
    else if (w_code == 2)
        info->status = WEATHER_PARTLY_CLOUDLY;
    else if (w_code == 3 || (w_code >= 45 && w_code <= 57))
        info->status = WEATHER_CLOUDLY;
    else if (w_code == 61 || w_code == 63 || w_code == 66)
        info->status = WEATHER_LIGHT_RAIN;
    else if (w_code == 65 || w_code == 67 || (w_code >= 80 && w_code <= 82))
        info->status = WEATHER_HEAVY_RAIN;
    else if ((w_code >= 71 && w_code <= 77) || w_code == 85 || w_code == 86)
        info->status = WEATHER_SNOWY;
    else if (w_code >= 95 && w_code <= 99)
        info->status = WEATHER_THUNDERSTORM;
    else {
        ESP_LOGW(TAG, "Unknown weather code %d, falling back to cloudly", w_code);
        info->status = WEATHER_CLOUDLY;
    }
    return 0;
}

static void draw_canvas(struct canvas *cv, struct image_desc *icons)
{
    uint8_t digits_x = cv->width - (digits_3x5_font.width * 2 + 1 + 4);  // 2 digits, 1px spacing, deg sign and 1px padding
    uint8_t digits_y = cv->height - digits_3x5_font.height;
    uint8_t deg_sign_x = cv->width - 3;

    // Draw icon
    cv_draw_image(cv, &icons[cur_weather.status], 0, 0);

    // Draw temperature
    int8_t temp = abs(cur_weather.temperature);
    bool is_negative = cur_weather.temperature < 0;
    if (temp >= 10)
        cv_draw_symbol(cv, &digits_3x5_font, temp / 10, digits_x, digits_y, color);
    cv_draw_symbol(cv, &digits_3x5_font, temp % 10, digits_x + digits_3x5_font.width + 1, digits_y, color);
    if (is_negative) {
        uint8_t minus_sign_x = (temp >= 10 ? (digits_x - 1) : (digits_x + digits_3x5_font.width)) - 3;
        cv_draw_line_h(cv, minus_sign_x, minus_sign_x + 2, digits_y + 2, color);
    }
    cv_draw_rect(cv, deg_sign_x, deg_sign_x + 2, digits_y - 1, digits_y + 1, color);
}

void weather_api_task(void *params)
{
    ESP_LOGI(TAG, "Created API task");
    while (1) {
        if (fetch_api(&cur_weather) != 0) {
            ESP_LOGE(TAG, "Failed to fetch forecast!");
            vTaskDelay(pdMS_TO_TICKS(API_FETCH_FAIL_DELAY));
            continue;
        }

        xTaskNotify(ui_task_handle, UI_MSG_UPDATE, eSetBits);
        vTaskDelay(pdMS_TO_TICKS(API_FETCH_DELAY));
    }
}

void weather_ui_task(void *param)
{
    struct canvas *cv = (struct canvas *)param;

    ui_task_handle = xTaskGetCurrentTaskHandle();

    uint8_t digits_x = cv->width - (digits_3x5_font.width * 2 + 1 + 4);  // 2 digits, 1px spacing, deg sign and 1px padding
    uint8_t digits_y = cv->height - digits_3x5_font.height;
    uint8_t deg_sign_x = cv->width - 3;

    // Preload all images
    struct image_desc icons[ICON_COUNT];
    struct image_desc img_desc;
    char path[64];
    for (uint8_t i = 0; i < ICON_COUNT; i++) {
        strcpy(path, base_path);
        strcat(path, weather_path);
        strcat(path, icon_files[i]);
        uint8_t ret = load_image_file(path, &img_desc);
        if (ret)
            while (1) { vTaskSuspend(NULL); }  // Halt task

        icons[i] = img_desc;
    }

    // Draw placeholder
    cv_draw_line_h(cv, digits_x, digits_x + 2, digits_y + 2, color);
    cv_draw_line_h(cv, digits_x + 4, digits_x + 4 + 2, digits_y + 2, color);
    cv_draw_rect(cv, deg_sign_x, deg_sign_x + 2, digits_y - 1, digits_y + 1, color);
    am_send_msg(AM_MSG_REFRESH);
    struct canvas *placeholder_cv = cv_copy(cv);

    // Start background task and display loading animation
    xTaskCreate(weather_api_task, "weather_background", 8 * 1024, NULL, 1, NULL);
    uint32_t message;
    bool anim_dir = 1;
    while (1) {
        if (anim_dir)
            anim_fade_in(cv, placeholder_cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
        else
            anim_fade_out(cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);

        if (xTaskNotifyWait(pdFALSE, ULONG_MAX, &message, 0) == pdTRUE)
            if (message & UI_MSG_UPDATE)
                break;
        
        anim_dir = !anim_dir;
    }

    if (anim_dir == 1) {
        // Fade out before displaying new data
        anim_fade_out(cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
    }
    struct canvas *temp_cv = cv_copy(cv);
    draw_canvas(temp_cv, icons);
    anim_fade_in(cv, temp_cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
    cv_free(temp_cv);

    struct weather_info prev_weather = cur_weather;
    while (1) {
        if (xTaskNotifyWait(pdFALSE, ULONG_MAX, &message, portMAX_DELAY) == pdFALSE || !(message & UI_MSG_UPDATE))
            continue;

        if (cur_weather.status != prev_weather.status || cur_weather.temperature != prev_weather.temperature) {
            anim_fade_out(cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
            struct canvas *temp_cv = cv_copy(cv);
            draw_canvas(temp_cv, icons);
            anim_fade_in(cv, temp_cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
            cv_free(temp_cv);
        }
        prev_weather = cur_weather;
    }
}
