/* Animations that can be used inside apps */

#include "freertos/FreeRTOS.h"
#include "animations.h"
#include "framebuffer.h"
#include "app_manager.h"

void anim_fade_out(struct canvas *cv, uint32_t duration_ms, uint32_t delay_ms)
{
    uint32_t max_brightness = duration_ms / delay_ms;
    uint32_t brightness = max_brightness;
    while (brightness-- > 0) {
        // Dim all pixels in canvas
        for (uint32_t i = 0; i < (cv->width * cv->height); i++)
            cv->buf[i] = crgb_mult(cv->buf[i], (float)brightness / (float)max_brightness);

        am_send_msg(AM_MSG_REFRESH);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void anim_fade_in(struct canvas *cv, struct canvas *cv_new, uint32_t duration_ms, uint32_t delay_ms)
{
    uint32_t max_brightness = duration_ms / delay_ms;
    uint32_t brightness = 0;
    while (brightness++ < max_brightness) {
        for (uint32_t i = 0; i < (cv->width * cv->height); i++)
            cv->buf[i] = crgb_mult(cv_new->buf[i], (float)brightness / (float)max_brightness);

        am_send_msg(AM_MSG_REFRESH);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
