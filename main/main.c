#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "led_strip.h"
#include "sdkconfig.h"

#include "framebuffer.h"
#include "apps.h"

// LED strip definitions
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

// Wi-Fi definitions
#define HC_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define HC_H2E_IDENTIFIER ""

#if CONFIG_HC_WIFI_AUTH_OPEN
#define HC_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_HC_WIFI_AUTH_WEP
#define HC_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_HC_WIFI_AUTH_WPA_PSK
#define HC_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_HC_WIFI_AUTH_WPA2_PSK
#define HC_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_HC_WIFI_AUTH_WPA_WPA2_PSK
#define HC_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_HC_WIFI_AUTH_WPA3_PSK
#define HC_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_HC_WIFI_AUTH_WPA2_WPA3_PSK
#define HC_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_HC_WIFI_AUTH_WAPI_PSK
#define HC_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
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

static int s_retry_num = 0;
bool wifi_is_connected = false;
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_is_connected = false;
        if (s_retry_num < CONFIG_HC_WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGW(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifi_is_connected = true;
    }
}

void init_wifi_sta(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi");
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_HC_WIFI_SSID,
            .password = CONFIG_HC_WIFI_PASSWD,
            .threshold.authmode = HC_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = HC_WIFI_SAE_MODE,
            .sae_h2e_identifier = HC_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    // Init LEDs and framebuffer
    configure_led();
    struct framebuffer *fb = fb_init(led_strip, CONFIG_HC_FB_BRIGHTNESS);

    // Init NVS and Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    if (strcmp(CONFIG_HC_WIFI_SSID, "") != 0)
        init_wifi_sta();
    else
        ESP_LOGW(TAG, "SSID is empty, skipping Wi-Fi init");

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
