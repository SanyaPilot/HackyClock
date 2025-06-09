#include <stdint.h>
#include <string.h>
#include "canvas.h"

static const crgb crgb_black = { 0, 0, 0 };

void cv_blank(struct canvas *cv)
{
    memset(cv->buf, 0, cv->width * cv->height * sizeof(crgb));
}

void cv_fill(struct canvas *cv, crgb color)
{
    for (uint8_t i = 0; i < cv->width * cv->height; i++)
        cv->buf[i] = color;
}

void cv_set_pixel(struct canvas *cv, uint8_t x, uint8_t y, crgb color)
{
    cv->buf[y * cv->width + x] = color;
}

void cv_draw_line_v(struct canvas *cv, uint8_t x, uint8_t y1, uint8_t y2, crgb color)
{
    for (uint8_t i = y1 * cv->width; i < y2 * cv->width; i += cv->width)
        cv->buf[i + x] = color;
}

void cv_draw_line_h(struct canvas *cv, uint8_t x1, uint8_t x2, uint8_t y, crgb color)
{
    uint8_t pos = y * cv->width;
    for (uint8_t j = x1; j < x2; j++)
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
