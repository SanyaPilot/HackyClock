#ifndef __IMG_UTILS_H__
#define __IMG_UTILS_H__

#include <stdint.h>

struct image_desc {
    uint8_t width;
    uint8_t height;
    uint8_t channels;
    uint8_t *pixels;
};

uint8_t load_image_file(const char *filename, struct image_desc *description);

#endif
