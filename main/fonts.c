#include "fonts.h"

static const uint8_t font3x5[] = {
    0b11111, 0b10001, 0b11111,    // 0
    0b10010, 0b11111, 0b10000,    // 1
    0b11001, 0b10101, 0b10011,    // 2
    0b10101, 0b10101, 0b11111,    // 3
    0b00111, 0b00100, 0b11111,    // 4
    0b10111, 0b10101, 0b11101,    // 5
    0b11111, 0b10101, 0b11101,    // 6
    0b11001, 0b00101, 0b00011,    // 7
    0b11111, 0b10101, 0b11111,    // 8
    0b10111, 0b10101, 0b11111,    // 9
};

const struct sym_range digits_ranges[] = {{ '0', '9' }, { /* sentinel */ }};
const struct bitmap_font digits_3x5_font = {
    .width = 3,
    .height = 5,
    .ranges = digits_ranges,
    .data = font3x5
};

static const uint8_t font7x7[] = {
    0b0111110, 0b1000001, 0b1000001, 0b1000001, 0b1000001, 0b1000001, 0b0111110,    // 0
    0b1001000, 0b1000100, 0b1000010, 0b1111111, 0b1000000, 0b1000000, 0b1000000,    // 1
    0b0110001, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b1000110,    // 2
    0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b0110110,    // 3
    0b0000111, 0b0001000, 0b0001000, 0b0001000, 0b0001000, 0b0001000, 0b1111111,    // 4
    0b1000110, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b0110001,    // 5
    0b0111110, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b0110000,    // 6
    0b0000001, 0b0000001, 0b0000001, 0b1100001, 0b0011001, 0b0000101, 0b0000011,    // 7
    0b0110110, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b0110110,    // 8
    0b0000110, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b1001001, 0b0111110,    // 9
};

const struct bitmap_font digits_7x7_font = {
    .width = 7,
    .height = 7,
    .ranges = digits_ranges,
    .data = font7x7
};

static const uint8_t spleen_5x8[] = {
    #include "spleen_5x8.h"
};

const struct sym_range spleen_5x8_ranges[] = {
    { ' ', '~' },       // ASCII
    { 1025, 1169 },     // Cyrillic
    { /* sentinel */ }
};
const struct bitmap_font spleen_5x8_font = {
    .width = 5,
    .height = 8,
    .ranges = spleen_5x8_ranges,
    .data = spleen_5x8
};
