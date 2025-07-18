#ifndef __CANVAS_H__
#define __CANVAS_H__

#include "framebuffer.h"
#include <stdint.h>
#include "fonts.h"
#include "image_utils.h"

struct canvas {
    uint8_t width;
    uint8_t height;
    crgb *buf;
};

struct canvas *cv_init(uint8_t width, uint8_t height);
struct canvas *cv_copy(struct canvas *old_cv);
void cv_free(struct canvas *cv);
void cv_blank(struct canvas *cv);
void cv_fill(struct canvas *cv, crgb color);
crgb cv_get_pixel(struct canvas *cv, uint8_t x, uint8_t y);
void cv_set_pixel(struct canvas *cv, uint8_t x, uint8_t y, crgb color);
void cv_draw_line_v(struct canvas *cv, uint8_t x, uint8_t y1, uint8_t y2, crgb color);
void cv_draw_line_h(struct canvas *cv, uint8_t x1, uint8_t x2, uint8_t y, crgb color);
void cv_draw_rect(struct canvas *cv, uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2, crgb color);
void cv_draw_symbol(struct canvas *cv, const struct bitmap_font *font, uint8_t sym_idx, uint8_t x, uint8_t y, crgb color);
void cv_draw_image(struct canvas *cv, struct image_desc *img, uint8_t x, uint8_t y);

#endif
