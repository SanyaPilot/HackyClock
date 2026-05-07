#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include "fonts.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "canvas.h"
#include "framebuffer.h"

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
    for (size_t i = 0; i < cv->width * cv->height; i++)
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

uint8_t cv_draw_symbol_utf8(struct canvas *cv, const struct bitmap_font *font, const char *sym, uint8_t x, uint8_t y, crgb color)
{
    // Simple UTF-8 decoder here
    uint32_t codepoint;
    uint8_t cp_size = 0;
    if ((sym[0] & 0x80) == 0) {
        // ASCII
        codepoint = sym[0];
        cp_size = 1;
    } else if ((sym[0] & 0xE0) == 0xC0) {   // 0x110...
        // 2 bytes
        codepoint = ((sym[0] & 0x1F) << 6) | (sym[1] & 0x3F);           // 0b110xxxyy 0b10yyzzzz -> 0b0xxxyyyyzzzz
        cp_size = 2;
    } else if ((sym[0] & 0xF0) == 0xE0) {   // 0x1110...
        // 3 bytes
        codepoint = ((sym[0] & 0x0F) << 12) | ((sym[1] & 0x3F) << 6) |  // 0b1110wwww 0b10xxxxyy 0b10yyzzzz -> 0bwwwwxxxxyyyyzzzz
                    (sym[2] & 0x3F);
        cp_size = 3;
    } else if ((sym[0] & 0xF8) == 0xF0) {   // 0x11110...
        // 4 bytes
        codepoint = ((sym[0] & 0x0F) << 18) | ((sym[1] & 0x3F) << 12) | // same but 22 bits
                    ((sym[2] & 0x3F) << 6)  | (sym[2] & 0x3F);
        cp_size = 4;
    } else
        return 0;

    cv_draw_symbol(cv, font, codepoint, x, y, color);
    return cp_size;
}

void cv_draw_symbol(struct canvas *cv, const struct bitmap_font *font, uint32_t sym, uint8_t x, uint8_t y, crgb color)
{
    // Find codepoint in our font by ranges
    uint32_t prev_end = 0, offset = 0;
    bool found = false;
    for (const struct sym_range *r = font->ranges; r->end != 0; r++) {
        offset += r->begin - prev_end;
        if (r->begin <= sym && sym <= r->end) {
            found = true;
            break;
        }
        prev_end = r->end + 1;
    }
    if (!found) {
        ESP_LOGW("text_dbg", "can't find symbol in the font: %d", sym);
        return;
    }
    ESP_LOGI("text_dbg", "offset = %d for sym %d", offset, sym);
    sym -= offset;
    // const uint8_t font_high_bit = 1 << (font->height - 1);
    const uint32_t start_pos = sym * font->width;
    for (uint8_t i = 0; i < font->width; i++) {
        if (x + i >= cv->width)
            break;
        uint8_t font_col = font->data[start_pos + i];
        for (uint8_t j = 0; j < font->height; j++) {
            if (y + j >= cv->height)
                break;
            cv_set_pixel(cv, x + i, y + j, font_col & (1 << j) ? color : crgb_black);
        }
    }
}

void cv_draw_text(struct canvas *cv, const struct bitmap_font *font, const char *string, uint8_t x, uint8_t y, crgb color)
{
    size_t len = strlen(string);
    uint8_t x_off = x;
    for (size_t i = 0; i < len; ) {
        uint8_t cp_size = cv_draw_symbol_utf8(cv, font, &string[i], x_off, y, color);
        if (!cp_size)
            return;
        i += cp_size;
        x_off += font->width;
        if (x_off >= cv->width)
            break;
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

// Copy contents of cv_child into cv
void cv_draw_cv(struct canvas *cv, struct canvas *cv_child, uint8_t x, uint8_t y)
{
    for (uint8_t i = 0; i < MIN(cv_child->height, cv->height - y); i++)
        for (uint8_t j = 0; j < MIN(cv_child->width, cv->width - x); j++)
            cv->buf[(y + i) * cv->width + (x + j)] = cv_child->buf[i * cv_child->width + j];
}

void cv_draw_to_fb(struct canvas *cv, struct framebuffer *fb, uint8_t x, uint8_t y)
{
    for (uint8_t i = 0; i < MIN(cv->height, CONFIG_HC_MATRIX_HEIGHT - y); i++)
        for (uint8_t j = 0; j < MIN(cv->width, CONFIG_HC_MATRIX_WIDTH - x); j++)
            fb->buf[y + i][x + j] = cv->buf[i * cv->width + j];

    fb_refresh(fb);
}
