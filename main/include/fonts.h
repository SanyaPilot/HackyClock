#ifndef __FONTS_H__
#define __FONTS_H__

#include <stdint.h>

#define DIGIT_TO_CHAR(x)    ((x) + '0')

struct sym_range
{
    uint32_t begin, end;
};

struct bitmap_font
{
    uint8_t width;
    uint8_t height;
    const struct sym_range *ranges;
    const uint8_t *data;
};

extern const struct bitmap_font digits_3x5_font;
extern const struct bitmap_font digits_7x7_font;
extern const struct bitmap_font spleen_5x8_font;
#endif
