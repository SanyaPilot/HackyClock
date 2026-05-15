#include <stdint.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "canvas.h"
#include "app_manager.h"
#include "framebuffer.h"
#include "animations.h"
#include "fonts.h"

#define FADE_ANIM_DURATION  300
#define FADE_ANIM_DELAY     20

enum clock_style {
    STYLE_SMALL = 0,
    STYLE_LARGE,
    STYLE_DATE,
    STYLE_COUNT
};

uint8_t start_x_sm, start_y_sm;
uint8_t start_x_lg, start_y_lg;
uint8_t start_x_wday, start_y_date;

const char* weekday[] = {"ВС", "ПН", "ВТ", "СР", "ЧТ", "ПТ", "СБ"};

static void draw_clock(struct canvas *cv, struct tm *tm, uint8_t style, crgb color)
{
    switch (style) {
        case STYLE_SMALL:
            // Draw hours
            cv_draw_symbol(cv, &digits_3x5_font, DIGIT_TO_CHAR(tm->tm_hour / 10),
                           start_x_sm, start_y_sm, color);
            cv_draw_symbol(cv, &digits_3x5_font, DIGIT_TO_CHAR(tm->tm_hour % 10),
                           start_x_sm + 1 + digits_3x5_font.width, start_y_sm, color);
            // Draw minutes
            cv_draw_symbol(cv, &digits_3x5_font, DIGIT_TO_CHAR(tm->tm_min / 10),
                           start_x_sm + 3 + digits_3x5_font.width * 2, start_y_sm, color);
            cv_draw_symbol(cv, &digits_3x5_font, DIGIT_TO_CHAR(tm->tm_min % 10),
                           start_x_sm + 4 + digits_3x5_font.width * 3, start_y_sm, color);
            break;
        
        case STYLE_LARGE:
            // Draw hours
            cv_draw_symbol(cv, &digits_7x7_font, DIGIT_TO_CHAR(tm->tm_hour / 10),
                           start_x_lg, start_y_lg, color);
            cv_draw_symbol(cv, &digits_7x7_font, DIGIT_TO_CHAR(tm->tm_hour % 10),
                           start_x_lg + 2 + digits_7x7_font.width, start_y_lg, color);
            // Draw minutes
            cv_draw_symbol(cv, &digits_7x7_font, DIGIT_TO_CHAR(tm->tm_min / 10),
                           start_x_lg, start_y_lg + 2 + digits_7x7_font.height, color);
            cv_draw_symbol(cv, &digits_7x7_font, DIGIT_TO_CHAR(tm->tm_min % 10),
                           start_x_lg + 2 + digits_7x7_font.width, start_y_lg + 2 + digits_7x7_font.height, color);
            break;
        case STYLE_DATE:
            // Draw a day
            cv_draw_symbol(cv, &digits_3x5_font, DIGIT_TO_CHAR(tm->tm_mday / 10),
                           start_x_sm, start_y_date, color);
            cv_draw_symbol(cv, &digits_3x5_font, DIGIT_TO_CHAR(tm->tm_mday % 10),
                           start_x_sm + 1 + digits_3x5_font.width, start_y_date, color);
            // Draw a month
            cv_draw_symbol(cv, &digits_3x5_font, DIGIT_TO_CHAR((tm->tm_mon + 1) / 10),
                           start_x_sm + digits_3x5_font.width * 2 + 3, start_y_date, color);
            cv_draw_symbol(cv, &digits_3x5_font, DIGIT_TO_CHAR((tm->tm_mon + 1) % 10),
                           start_x_sm + digits_3x5_font.width * 3 + 4, start_y_date, color);
            // Draw a weekday
            cv_draw_text(cv, &spleen_5x8_font, weekday[tm->tm_wday],
                         start_x_wday, start_y_date + digits_3x5_font.height + 1, color);
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

    // Date style
    start_x_wday = (cv->width - ((spleen_5x8_font.width - 1) * 2)) / 2;
    start_y_date = (cv->width - (digits_3x5_font.height + spleen_5x8_font.height - 1)) / 2;

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
            style = (style + 1) % STYLE_COUNT;
            anim_fade_out(cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
            struct canvas *temp_cv = cv_copy(cv);
            draw_clock(temp_cv, &timeinfo, style, color);
            anim_fade_in(cv, temp_cv, FADE_ANIM_DURATION, FADE_ANIM_DELAY);
            cv_free(temp_cv);
        }
    }
}
