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

void scrolling_text(struct canvas *cv, struct canvas *cv_text, uint32_t duration_ms, uint32_t delay_ms)
{
    uint8_t offset = 0;
    uint8_t step = 1;
    uint8_t pixel_x;
    // Change condition
    while(1) 
    {
        for (uint8_t y = 0; y < cv->height; y++) {
            for (uint8_t x = 0; x < cv->width; x++)
            {
                pixel_x = offset + x;
                if (x < cv_text->width) {
                    crgb pixel = cv_text->buf[y * cv_text->width + pixel_x];
                    cv->buf[y * cv->width + x] = pixel;
                }
            }
        }
        am_send_msg(AM_MSG_REFRESH);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        offset += step;        
        if (offset > cv_text->width - cv->width) {
            for (uint8_t i = 0; i < cv->height; i++) {
                cv->buf[i] = crgb_mult(cv->buf[i], 0);
            }
            offset = 0;
            anim_fade_out(cv, duration_ms, delay_ms);
        } 
    }
}