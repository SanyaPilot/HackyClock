#ifndef __FONTS_H__
#define __FONTS_H__

#include <stdint.h>

struct bitmap_font
{
    uint8_t width;
    uint8_t height;
    const uint8_t *data;
};

extern const struct bitmap_font digits_3x5_font;
extern const struct bitmap_font digits_7x7_font;
#endif
