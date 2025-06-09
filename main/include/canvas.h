#ifndef __CANVAS_H__
#define __CANVAS_H__

#include "framebuffer.h"
#include <stdint.h>

struct canvas {
    uint8_t width;
    uint8_t height;
    crgb *buf;
};

struct image_desc {
    uint8_t width;
    uint8_t height;
    uint8_t channels;
    uint8_t *pixels;
};

void cv_blank(struct canvas *cv);
void cv_fill(struct canvas *cv, crgb color);
void cv_set_pixel(struct canvas *cv, uint8_t x, uint8_t y, crgb color);
void cv_draw_line_v(struct canvas *cv, uint8_t x, uint8_t y1, uint8_t y2, crgb color);
void cv_draw_line_h(struct canvas *cv, uint8_t x1, uint8_t x2, uint8_t y, crgb color);
void cv_draw_rect(struct canvas *cv, uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2, crgb color);

#endif
