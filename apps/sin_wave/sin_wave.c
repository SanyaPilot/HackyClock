#include <stdint.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "app_manager.h"
#include "canvas.h"

#define TAG "sin_wave"

#define SIN_FREQ            0.25
#define SIN_AMP             3
#define SIN_Y_OFFSET        7
#define SIN_FRAME_DELAY     50
#define SIN_MIN_BRIGHTNESS  31
#define SIN_MAX_BRIGHTNESS  248

void sin_wave_ui_task(void *param)
{
    struct canvas *cv = (struct canvas *)param;

    // Draw sin wave
    ESP_LOGI(TAG, "Drawing sin wave");

    // Calculate wave
    size_t sin_size = (size_t) ceil(2 * M_PI / SIN_FREQ);
    int8_t *sin_data = pvPortMalloc(sin_size * sizeof(int8_t));
    int8_t min_neg_val = 0;
    for (size_t x = 0; x < sin_size; x++)
    {
        sin_data[x] = (int8_t) round(SIN_AMP * sin(SIN_FREQ * x)) + SIN_Y_OFFSET;
        if (sin_data[x] < min_neg_val)
            min_neg_val = sin_data[x];
    }
    // Shift all values to remove negative ones
    for (size_t x = 0; x < sin_size; x++)
    {
        sin_data[x] -= min_neg_val;
        // Cap max value
        if (sin_data[x] >= cv->height)
            sin_data[x] = cv->height - 1;
    }

    uint8_t x_off = 0;
    while (1)
    {
        cv_blank(cv);
        // Draw wave
        for (uint8_t x = 0; x < cv->width; x++)
        {
            size_t data_idx = x + x_off;
            if (data_idx >= sin_size)
                data_idx -= sin_size;

            uint8_t brightness = SIN_MIN_BRIGHTNESS;
            for (uint8_t y = sin_data[data_idx]; y < cv->height; y++)
            {
                crgb cur_color = { .r = 0, .g = 0, .b = brightness };
                cv_set_pixel(cv, x, y, cur_color);
                if (brightness < SIN_MAX_BRIGHTNESS)
                    brightness <<= 1;
            }
        }
        am_send_msg(AM_MSG_REFRESH);

        // Shift X
        if (++x_off == sin_size)
            x_off -= sin_size;

        ESP_LOGI(TAG, "Draw frame!");
        vTaskDelay(pdMS_TO_TICKS(SIN_FRAME_DELAY));
    }
}
