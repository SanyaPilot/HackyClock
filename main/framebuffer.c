#include <stdint.h>
#include <string.h>
#include "framebuffer.h"
#include "led_strip.h"
#include "sdkconfig.h"

static uint16_t matrix_led_index(uint8_t x, uint8_t y)
{
    // First LED is in the top-right corner
    // In serpetine matrix pixels are laid out back-and-forth
    uint8_t rowX = (y & CONFIG_HC_MATRIX_SERPENTINE_DIR_INVERSED) ? x : (CONFIG_HC_MATRIX_WIDTH - x - 1);
    return y * CONFIG_HC_MATRIX_HEIGHT + rowX;
}

static crgb crgb_mult(crgb a, float mult)
{
    crgb res = { .r = a.r * mult, .g = a.g * mult, .b = a.b * mult };
    return res;
}

struct framebuffer *fb_init(led_strip_handle_t led_strip, uint8_t brightness)
{
    struct framebuffer *fb = pvPortMalloc(sizeof(struct framebuffer));
    fb->led_strip = led_strip;
    fb->brightness = brightness;
    fb_blank(fb);
    return fb;
}

void fb_blank(struct framebuffer *fb)
{
    memset(fb->buf, 0, CONFIG_HC_MATRIX_HEIGHT * CONFIG_HC_MATRIX_WIDTH * sizeof(crgb));
}

void fb_fill(struct framebuffer *fb, crgb color)
{
    for (uint8_t y = 0; y < CONFIG_HC_MATRIX_HEIGHT; y++)
        for (uint8_t x = 0; x < CONFIG_HC_MATRIX_WIDTH; x++)
            fb->buf[y][x] = color;
}

void fb_refresh(struct framebuffer *fb)
{
    for (uint8_t y = 0; y < CONFIG_HC_MATRIX_HEIGHT; y++)
        for (uint8_t x = 0; x < CONFIG_HC_MATRIX_WIDTH; x++) {
            // Apply brightness
            crgb pixel = crgb_mult(fb->buf[y][x], fb->brightness / 255.f);
            led_strip_set_pixel(fb->led_strip, matrix_led_index(x, y), pixel.r, pixel.g, pixel.b);
        }
    led_strip_refresh(fb->led_strip);
}
