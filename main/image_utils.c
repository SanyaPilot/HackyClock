#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "image_utils.h"

// QOI image decoder
#define QOI_IMPLEMENTATION
#define QOI_MALLOC(sz) pvPortMalloc(sz)
#define QOI_FREE(p)    vPortFree(p)
#include "qoi.h"

static const char *TAG = "image_utils";

// Tries to read and decode image from file
// Only .qoi format is supported for now
uint8_t load_image_file(const char *filename, struct image_desc *description)
{
    char f_ext[4];
    strncpy(f_ext, filename + strlen(filename) - 3, 4);
    if (strcmp(f_ext, "qoi") == 0) {
        qoi_desc qoi_desc;
        void *rgba_pixels = qoi_read(filename, &qoi_desc, 4);
        if (rgba_pixels == NULL) {
            ESP_LOGE(TAG, "[load_image_file] Failed to load %s!", filename);
            return 1;
        }
        description->width = qoi_desc.width;
        description->height = qoi_desc.height;
        description->channels = qoi_desc.channels;
        description->pixels = rgba_pixels;
        return 0;
    } else {
        ESP_LOGE(TAG, "[load_image_file] Image format .%s is not supported!", f_ext);
    }
    return 1;
}
