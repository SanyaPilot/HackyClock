#ifndef __FRAMEBUFFER_H__
#define __FRAMEBUFFER_H__

#include <stdint.h>
#include "led_strip_types.h"
#include "sdkconfig.h"

typedef struct crgb
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} crgb;

struct framebuffer
{
    crgb buf[CONFIG_HC_MATRIX_HEIGHT][CONFIG_HC_MATRIX_WIDTH];
    led_strip_handle_t led_strip;
    uint8_t brightness;
};

struct framebuffer *fb_init(led_strip_handle_t led_strip, uint8_t brightness);
void fb_blank(struct framebuffer *fb);
void fb_fill(struct framebuffer *fb, crgb color);
void fb_refresh(struct framebuffer *fb);

crgb crgb_mult(crgb a, float mult);

#endif