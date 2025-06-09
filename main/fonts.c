#include "fonts.h"

static const uint8_t font3x5[] = {
    0b11111, 0b10001, 0b11111,    // 0
    0b01001, 0b11111, 0b00001,    // 1
    0b10011, 0b10101, 0b11001,    // 2
    0b10101, 0b10101, 0b11111,    // 3
    0b11100, 0b00100, 0b11111,    // 4
    0b11101, 0b10101, 0b10111,    // 5
    0b11111, 0b10101, 0b10111,    // 6
    0b10011, 0b10100, 0b11000,    // 7
    0b11111, 0b10101, 0b11111,    // 8
    0b11101, 0b10101, 0b11111,    // 9
};

const struct bitmap_font digits_3x5_font = {
    .width = FONT_WIDTH,
    .height = FONT_HEIGHT,
    .data = font3x5
};
