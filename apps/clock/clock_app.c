#include <stdint.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "canvas.h"
#include "app_manager.h"
#include "framebuffer.h"
#include "fonts.h"

enum clock_style {
    STYLE_SMALL = 0,
    STYLE_LARGE
};

uint8_t start_x_sm, start_y_sm;
uint8_t start_x_lg, start_y_lg;

static void draw_clock(struct canvas *cv, struct tm *tm, uint8_t style, crgb color)
{
    switch (style) {
        case STYLE_SMALL:
            // Draw hours
            cv_draw_symbol(cv, &digits_3x5_font, tm->tm_hour / 10,
                           start_x_sm, start_y_sm, color);
            cv_draw_symbol(cv, &digits_3x5_font, tm->tm_hour % 10,
                           start_x_sm + 1 + digits_3x5_font.width, start_y_sm, color);
            // Draw minutes
            cv_draw_symbol(cv, &digits_3x5_font, tm->tm_min / 10,
                           start_x_sm + 3 + digits_3x5_font.width * 2, start_y_sm, color);
            cv_draw_symbol(cv, &digits_3x5_font, tm->tm_min % 10,
                           start_x_sm + 4 + digits_3x5_font.width * 3, start_y_sm, color);
            break;
        
        case STYLE_LARGE:
            // Draw hours
            cv_draw_symbol(cv, &digits_7x7_font, tm->tm_hour / 10,
                           start_x_lg, start_y_lg, color);
            cv_draw_symbol(cv, &digits_7x7_font, tm->tm_hour % 10,
                           start_x_lg + 2 + digits_7x7_font.width, start_y_lg, color);
            // Draw minutes
            cv_draw_symbol(cv, &digits_7x7_font, tm->tm_min / 10,
                           start_x_lg, start_y_lg + 2 + digits_7x7_font.height, color);
            cv_draw_symbol(cv, &digits_7x7_font, tm->tm_min % 10,
                           start_x_lg + 2 + digits_7x7_font.width, start_y_lg + 2 + digits_7x7_font.height, color);
            break;
    }

    am_send_msg(AM_MSG_REFRESH);
}

void clock_ui_task(void *param)
{
    struct canvas *cv = (struct canvas *)param;

    time_t now;
    struct tm timeinfo, prev_timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Small style
    // 4 digits, 2 spaces (1 pixel each), 1 space between hours and minutes (2 pixels)
    start_x_sm = (cv->width - (digits_3x5_font.width * 4 + 4)) / 2;
    start_y_sm = (cv->height - digits_3x5_font.height) / 2;

    // Large style
    start_x_lg = (cv->width - (digits_7x7_font.width * 2 + 2)) / 2;
    start_y_lg = (cv->height - (digits_7x7_font.height * 2 + 2)) / 2;

    const crgb color = {255, 255, 255};
    uint8_t style = STYLE_SMALL;

    draw_clock(cv, &timeinfo, style, color);

    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_hour != prev_timeinfo.tm_hour ||
            timeinfo.tm_min != prev_timeinfo.tm_min) {
            draw_clock(cv, &timeinfo, style, color);
        }
        prev_timeinfo = timeinfo;

        // Process input events
        uint32_t message;
        if (xTaskNotifyWait(pdFALSE, ULONG_MAX, &message, pdMS_TO_TICKS(1000)) == pdFALSE)
            continue;

        if (message == EVENT_BTN_CLICK) {
            // Switch clock style
            style = !style;
            cv_blank(cv);
            draw_clock(cv, &timeinfo, style, color);
        }
    }
}
