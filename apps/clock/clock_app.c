#include <stdint.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "canvas.h"
#include "app_manager.h"
#include "framebuffer.h"
#include "fonts.h"

static void draw_clock(struct canvas *cv, struct tm *tm, uint8_t start_x, uint8_t y, crgb color)
{
    // Draw hours
    cv_draw_symbol(cv, &digits_3x5_font, tm->tm_hour / 10, start_x, y, color);
    cv_draw_symbol(cv, &digits_3x5_font, tm->tm_hour % 10, start_x + 1 + digits_3x5_font.width, y, color);
    // Draw minutes
    cv_draw_symbol(cv, &digits_3x5_font, tm->tm_min / 10, start_x + 3 + digits_3x5_font.width * 2, y, color);
    cv_draw_symbol(cv, &digits_3x5_font, tm->tm_min % 10, start_x + 4 + digits_3x5_font.width * 3, y, color);

    am_send_msg(AM_MSG_REFRESH);
}

void clock_ui_task(void *param)
{
    struct canvas *cv = (struct canvas *)param;

    time_t now;
    struct tm timeinfo, prev_timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Total width of clock
    // 4 digits, 2 spaces (1 pixel each), 1 space between hours and minutes (2 pixels)
    const uint8_t clock_width = digits_3x5_font.width * 4 + 4;
    const uint8_t start_x = (cv->width - clock_width) / 2;
    const uint8_t y = (cv->height - digits_3x5_font.height) / 2;
    const crgb color = {255, 255, 255};

    draw_clock(cv, &timeinfo, start_x, y, color);

    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_hour != prev_timeinfo.tm_hour ||
            timeinfo.tm_min != prev_timeinfo.tm_min) {
            draw_clock(cv, &timeinfo, start_x, y, color);
        }
        prev_timeinfo = timeinfo;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
