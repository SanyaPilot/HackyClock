#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "app_manager.h"
#include "sdkconfig.h"

#define MIN_SWIPE_DELAY CONFIG_HC_INP_MIN_SWIPE_DELAY * 1000

#define GPIO_COUNT  3
static const int8_t button_gpios[] = {
    CONFIG_HC_INP_LEFT_BUTTON_GPIO,
    CONFIG_HC_INP_CENTER_BUTTON_GPIO,
    CONFIG_HC_INP_RIGHT_BUTTON_GPIO
};

#define INPUT_TASK_STACK_SIZE   (2 * 1024)
#define INPUT_TASK_PRIORITY     10

enum input_states {
    STATE_RELEASED,
    STATE_PRESSED,
    STATE_SWIPE_L,
    STATE_SWIPE_R
};

struct gpio_event {
    uint8_t gpio_num;
    uint8_t level;
};

static const char *TAG = "input";
static QueueHandle_t gpio_events = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint8_t gpio_num = (uint32_t) arg;
    struct gpio_event evt = { .gpio_num = gpio_num, .level = gpio_get_level(gpio_num) };
    xQueueSendFromISR(gpio_events, &evt, NULL);
}

static void gpio_events_handler_task(void* arg)
{
    struct gpio_event evt, prev_evt;
    enum input_states state = STATE_RELEASED;
    while (1) {
        switch (state) {
            case STATE_RELEASED:
                // Nothing is touched, wait for touch event and go to STATE_PRESSED
                ESP_LOGD(TAG, "STATE_RELEASED");
                if (xQueueReceive(gpio_events, &evt, portMAX_DELAY) == pdFALSE)
                    continue;
                if (evt.level == 1)
                    state = STATE_PRESSED;
                break;
            
            case STATE_PRESSED:
                // Something is pressed, wait for further events and decide
                ESP_LOGD(TAG, "STATE_PRESSED");
                prev_evt = evt;
                uint64_t start_time = esp_timer_get_time();
                if (xQueueReceive(gpio_events, &evt, portMAX_DELAY) == pdFALSE) {
                    state = STATE_RELEASED;
                    continue;
                }
                uint64_t end_time = esp_timer_get_time();
                if (evt.level == 0) {
                    if (evt.gpio_num == prev_evt.gpio_num &&
                        evt.gpio_num != CONFIG_HC_INP_CENTER_BUTTON_GPIO &&
                        end_time - start_time > MIN_SWIPE_DELAY) {
                        // Same (L or R) button was released, check for a swipe
                        struct gpio_event temp_evt;
                        if (xQueueReceive(gpio_events, &temp_evt, pdMS_TO_TICKS(CONFIG_HC_INP_GESTURE_TIMEOUT))) {
                            if (temp_evt.gpio_num == CONFIG_HC_INP_CENTER_BUTTON_GPIO && temp_evt.level == 1) {
                                // It's a swipe
                                state = (evt.gpio_num == CONFIG_HC_INP_RIGHT_BUTTON_GPIO) ? STATE_SWIPE_L : STATE_SWIPE_R;
                                break;
                            }
                        }
                    }
                    // Emit click event
                    am_send_input_event(EVENT_BTN_CLICK);
                    state = STATE_RELEASED;
                } else if (evt.gpio_num != prev_evt.gpio_num && evt.gpio_num == CONFIG_HC_INP_CENTER_BUTTON_GPIO &&
                           evt.level == 1 && end_time - start_time > MIN_SWIPE_DELAY) {
                    // Center button was pressed with L or R buttons, it's a swipe
                   state = (prev_evt.gpio_num == CONFIG_HC_INP_RIGHT_BUTTON_GPIO) ? STATE_SWIPE_L : STATE_SWIPE_R;
                }
                break;
            
            case STATE_SWIPE_L:
            case STATE_SWIPE_R:
                ESP_LOGD(TAG, "STATE_SWIPE");
                // Two buttons were already pressed, wait for third press during timeout
                if (xQueueReceive(gpio_events, &evt, pdMS_TO_TICKS(CONFIG_HC_INP_GESTURE_TIMEOUT)) == pdFALSE) {
                    // Discard event
                    ESP_LOGD(TAG, "Discaring event!");
                    state = STATE_RELEASED;
                    continue;
                }
                // Ignore button releases
                if (evt.level == 0)
                    continue;
                
                if (state == STATE_SWIPE_L && evt.gpio_num == CONFIG_HC_INP_LEFT_BUTTON_GPIO)
                    // Emit left swipe event
                    am_send_msg(AM_MSG_NEXTAPP);
                else if (state == STATE_SWIPE_R && evt.gpio_num == CONFIG_HC_INP_RIGHT_BUTTON_GPIO)
                    // Emit right swipe event
                    am_send_msg(AM_MSG_PREVAPP);

                state = STATE_RELEASED;
                break;
        }
    }
}

void init_gpio_buttons(void)
{
    uint32_t pin_mask = 0;
    for (uint8_t i = 0; i < GPIO_COUNT; i++) {
        pin_mask |= (1 << button_gpios[i]);
    }
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf);
    gpio_events = xQueueCreate(8, sizeof(struct gpio_event));
    xTaskCreate(gpio_events_handler_task, "gpio_event_handler", INPUT_TASK_STACK_SIZE, NULL, INPUT_TASK_PRIORITY, NULL);

    gpio_install_isr_service(0);
    for (uint8_t i = 0; i < GPIO_COUNT; i++) {
        gpio_isr_handler_add(button_gpios[i], gpio_isr_handler, (void*) (uint32_t) button_gpios[i]);
    }
}
