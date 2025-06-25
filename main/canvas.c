#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "canvas.h"

static const crgb crgb_black = { 0, 0, 0 };
static const char *TAG = "canvas";

struct canvas *cv_init(uint8_t width, uint8_t height)
{
    struct canvas *new_cv = pvPortMalloc(sizeof(struct canvas));
    new_cv->width = width;
    new_cv->height = height;
    new_cv->buf = pvPortMalloc(sizeof(crgb) * width * height);
    cv_blank(new_cv);
    return new_cv;
}

struct canvas *cv_copy(struct canvas *old_cv)
{
    struct canvas *new_cv = pvPortMalloc(sizeof(struct canvas));
    new_cv->width = old_cv->width;
    new_cv->height = old_cv->height;
    size_t buf_size = sizeof(crgb) * old_cv->width * old_cv->height;
    new_cv->buf = pvPortMalloc(buf_size);
    memcpy(new_cv->buf, old_cv->buf, buf_size);
    return new_cv;
}

void cv_free(struct canvas *cv)
{
    free(cv->buf);
    free(cv);
}

void cv_blank(struct canvas *cv)
{
    memset(cv->buf, 0, cv->width * cv->height * sizeof(crgb));
}

void cv_fill(struct canvas *cv, crgb color)
{
    for (uint8_t i = 0; i < cv->width * cv->height; i++)
        cv->buf[i] = color;
}

crgb cv_get_pixel(struct canvas *cv, uint8_t x, uint8_t y)
{
    return cv->buf[y * cv->width + x];
}

void cv_set_pixel(struct canvas *cv, uint8_t x, uint8_t y, crgb color)
{
    cv->buf[y * cv->width + x] = color;
}

void cv_draw_line_v(struct canvas *cv, uint8_t x, uint8_t y1, uint8_t y2, crgb color)
{
    for (uint8_t i = y1 * cv->width; i <= y2 * cv->width; i += cv->width)
        cv->buf[i + x] = color;
}

void cv_draw_line_h(struct canvas *cv, uint8_t x1, uint8_t x2, uint8_t y, crgb color)
{
    uint8_t pos = y * cv->width;
    for (uint8_t j = x1; j <= x2; j++)
        cv->buf[pos + j] = color;
}

void cv_draw_rect(struct canvas *cv, uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2, crgb color)
{
    cv_draw_line_h(cv, x1, x2, y1, color);
    cv_draw_line_h(cv, x1, x2, y2, color);
    cv_draw_line_v(cv, x1, y1, y2, color);
    cv_draw_line_v(cv, x2, y1, y2, color);
}

void cv_draw_symbol(struct canvas *cv, const struct bitmap_font *font, uint8_t sym_idx, uint8_t x, uint8_t y, crgb color)
{
    const uint8_t font_high_bit = 1 << (font->height - 1);
    const uint8_t start_pos = sym_idx * font->width;
    for (uint8_t i = 0; i < font->width; i++) {
        uint8_t font_col = font->data[start_pos + i];
        for (uint8_t j = 0; j < font->height; j++)
            cv_set_pixel(cv, x + i, y + j, font_col & (font_high_bit >> j) ? color : crgb_black);
    }
}

// Draws image (raw RGB888 or RGBA8888 pixels)
void cv_draw_image(struct canvas *cv, struct image_desc *img, uint8_t x, uint8_t y)
{
    if (img->channels != 3 && img->channels != 4) {
        ESP_LOGE(TAG, "Channel count must be 3 (RGB) or 4 (RGBA)");
        return;
    }
    // Convert raw bytes into crgb (ignore alpha for now), and fill canvas
    for (uint8_t i = 0; i < (cv->height < img->height ? cv->height : img->height); i++) {
        if (y + i >= cv->height)
            break;
        for (uint8_t j = 0; j < (cv->width < img->width ? cv->width : img->width); j++) {
            if (x + j >= cv->width)
                break;
            size_t pos = (i * img->width + j) * img->channels;
            uint8_t *pixels = img->pixels;
            crgb final;
            if (img->channels == 3) {
                final.r = pixels[pos]; final.g = pixels[pos + 1]; final.b = pixels[pos + 2];
            } else {
                // Blend alpha (assume that alpha of canvas pixels is 1.0)
                crgb cv_px = cv_get_pixel(cv, x + j, y + i);
                float alpha = pixels[pos + 3] / 255.f;
                final.r = pixels[pos] * alpha + cv_px.r * (1 - alpha);
                final.g = pixels[pos + 1] * alpha + cv_px.g * (1 - alpha);
                final.b = pixels[pos + 2] * alpha + cv_px.b * (1 - alpha);
            }
            cv_set_pixel(cv, x + j, y + i, final);
        }
    }
}
