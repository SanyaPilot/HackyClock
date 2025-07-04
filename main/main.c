#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif_sntp.h"
#include "esp_vfs_fat.h"
#include "iot_button.h"
#include "nvs_flash.h"
#include "driver/pulse_cnt.h"
#include "led_strip.h"
#include "button_gpio.h"
#include "sdkconfig.h"

#include "framebuffer.h"
#include "app_manager.h"
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

const char *base_path = "/spiflash";
static wl_handle_t wl_handle = WL_INVALID_HANDLE;
static led_strip_handle_t led_strip;
static button_handle_t gpio_btn = NULL;

static int wifi_retry_num = 0;
bool wifi_is_connected = false;

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

static void configure_storage(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false
    };

    // Mount FATFS filesystem located on "storage" partition in read-write mode
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_is_connected = false;
        if (wifi_retry_num < CONFIG_HC_WIFI_MAX_RETRY) {
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGW(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
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

static void init_sntp(void)
{
    // Set time zone
    setenv("TZ", CONFIG_HC_TIME_ZONE, 1);
    tzset();
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_HC_NTP_SERVER);
    esp_netif_sntp_init(&config);
}

// Encoder via PCNT peripheral
static bool pcnt_interrupt_handler(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    // If the button is pressed, send input event to the app, otherwise switch apps
    if (iot_button_get_key_level(gpio_btn))
        am_send_input_event((edata->watch_point_value > 0) ? EVENT_KNOB_RIGHT : EVENT_KNOB_LEFT, &high_task_wakeup);
    else
        am_send_msg_from_isr((edata->watch_point_value > 0) ? AM_MSG_NEXTAPP : AM_MSG_PREVAPP, &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

static void init_encoder_pcnt(void)
{
    pcnt_unit_config_t unit_config = {
        .high_limit = 4,
        .low_limit = -4,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = CONFIG_HC_ENCODER_PIN_A,
        .level_gpio_num = CONFIG_HC_ENCODER_PIN_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = CONFIG_HC_ENCODER_PIN_B,
        .level_gpio_num = CONFIG_HC_ENCODER_PIN_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    int watch_points[] = {-4, 4};
    for (size_t i = 0; i < sizeof(watch_points) / sizeof(watch_points[0]); i++) {
        ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, watch_points[i]));
    }
    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_interrupt_handler,
    };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, NULL));
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

static void button_click_callback(void *handle, void *usr_data)
{
    // IDK if this works... Maybe I should rewrite button code by my own?
    am_send_input_event(EVENT_BTN_CLICK, NULL);
}

static void init_button(void)
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = CONFIG_HC_BUTTON_GPIO,
        .active_level = 0,
    };
    iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
    if (gpio_btn == NULL)
        ESP_LOGE(TAG, "Button create failed");

    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, NULL, button_click_callback, NULL);
}

void app_main(void)
{
    // Storage
    configure_storage();

    // Init NVS and Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (strcmp(CONFIG_HC_WIFI_SSID, "") != 0) {
        init_sntp();
        init_wifi_sta();
    } else
        ESP_LOGW(TAG, "SSID is empty, skipping Wi-Fi init");

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

    // Setup PCNT to catch encoder events and button driver
    init_button();
    init_encoder_pcnt();
}
