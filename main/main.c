#include "freertos/FreeRTOS.h"
#include "led_strip.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "framebuffer.h"
#include "apps.h"

#if CONFIG_HC_STRIP_LED_TYPE_WS2812
#define HC_LED_TYPE     LED_MODEL_WS2812
#elif CONFIG_HC_STRIP_LED_TYPE_WS2811
#define HC_LED_TYPE     LED_MODEL_WS2811
#elif CONFIG_HC_STRIP_LED_TYPE_SK6812
#define HC_LED_TYPE     LED_MODEL_SK6812
#endif

#if CONFIG_HC_STRIP_COLOR_ORDER_GRB
#define HC_COLOR_ORDER  LED_STRIP_COLOR_COMPONENT_FMT_GRB
#elif CONFIG_HC_STRIP_COLOR_ORDER_RGB
#define HC_COLOR_ORDER  LED_STRIP_COLOR_COMPONENT_FMT_RGB
#endif

static const char *TAG = "main";

static led_strip_handle_t led_strip;

static void configure_led(void)
{
    ESP_LOGI(TAG, "Configuring LED strip");
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_HC_STRIP_GPIO,
        .max_leds = CONFIG_HC_MATRIX_HEIGHT * CONFIG_HC_MATRIX_WIDTH,
        .led_model = HC_LED_TYPE,
        .color_component_format = HC_COLOR_ORDER,
        .flags = {
            .invert_out = false,
        }
    };
    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI2_HOST,
        .flags = {
            .with_dma = true,
        }
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
    // Set all LED off to clear all pixels
    led_strip_clear(led_strip);
}

void app_main(void)
{
    // Init LEDs and framebuffer
    configure_led();
    struct framebuffer *fb = fb_init(led_strip, CONFIG_HC_FB_BRIGHTNESS);

    // Draw some lines
    ESP_LOGI(TAG, "Drawing square");
    crgb color = { .r = 255, .g = 0, .b = 0 };
    for (uint8_t x = 0; x < CONFIG_HC_MATRIX_WIDTH; x++)
    {
        fb->buf[0][x] = color;
        fb->buf[CONFIG_HC_MATRIX_HEIGHT - 1][x] = color;
    }
    for (uint8_t y = 0; y < CONFIG_HC_MATRIX_HEIGHT; y++)
    {
        fb->buf[y][0] = color;
        fb->buf[y][CONFIG_HC_MATRIX_WIDTH - 1] = color;
    }
    fb_refresh(fb);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Launch app manager
    ESP_LOGI(TAG, "Starting app manager");
    struct am_params *am_params = pvPortMalloc(sizeof(struct am_params));
    am_params->framebuffer = fb;
    am_params->apps = registered_apps;
    launch_am_task(am_params);
}
