#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "app_manager.h"
#include "canvas.h"
#include "framebuffer.h"
#include "sdkconfig.h"

#define TAG "app_manager"

#define AM_STACK_SIZE      (configMINIMAL_STACK_SIZE * 2)
#define AM_TASK_PRIORITY   (tskIDLE_PRIORITY + 2)
#define AM_WIN_PRIORITY    (tskIDLE_PRIORITY + 1)

struct am_window_data
{
    TaskHandle_t handle;
    struct canvas *canvas;
};

static TaskHandle_t am_handle = NULL;
static struct am_window_data win_data[CONFIG_HC_AM_MAX_APPS];
static uint8_t win_idx = 0;

static void redraw_fb(struct framebuffer *fb, struct canvas *cv)
{
    // TODO: rewrite framebuffer and remove this shit
    for (uint8_t y = 0; y < cv->height; y++)
        for (uint8_t x = 0; x < cv->width; x++)
            fb->buf[y][x] = cv->buf[y * cv->width + x];

    fb_refresh(fb);
}

static void play_slide_anim(struct framebuffer *fb, struct canvas *cv_l, struct canvas *cv_r, bool dir)
{
    uint8_t width = cv_l->width;
    uint8_t height = cv_l->height;
    uint8_t off_start = dir ? 1 : (width - 1);
    uint8_t off_end = dir ? width : 0;

    for (int8_t off = off_start; (dir ? off <= off_end: off >= off_end); (dir ? off++ : off--)) {
        // Draw the left canvas with start offset
        for (uint8_t y = 0; y < height; y++)
            for (uint8_t x = off; x < width; x++)
                fb->buf[y][x - off] = cv_l->buf[y * width + x];
        
        // Draw the right canvas on the rest of FB
        for (uint8_t y = 0; y < height; y++)
            for (uint8_t x = 0; x < off; x++)
                fb->buf[y][width - off + x] = cv_r->buf[y * width + x];

        fb_refresh(fb);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_HC_AM_SLIDE_FRAME_DELAY));
    }
}

static int launch_window_task(struct am_app_info info, struct am_window_data *data)
{
    ESP_LOGI(TAG, "Launching %s task", info.name);
    return xTaskCreate(
        info.ui_task,
        info.name,
        configMINIMAL_STACK_SIZE * 2,
        data->canvas,
        AM_WIN_PRIORITY,
        &data->handle
    );
}

static void window_manager_task(void *param)
{
    struct am_params *params = (struct am_params *)param;
    struct am_app_info *app_info = params->apps;
    struct framebuffer *fb = params->framebuffer;

    // Allocate memory for canvases
    ESP_LOGI(TAG, "Available windows:");
    uint8_t win_count = 0;
    for (uint8_t i = 0; (void *)(app_info[i].name) != NULL; i++) {
        struct canvas *new_cv = pvPortMalloc(sizeof(struct canvas));
        new_cv->width = CONFIG_HC_MATRIX_WIDTH;
        new_cv->height = CONFIG_HC_MATRIX_HEIGHT;
        new_cv->buf = pvPortMalloc(sizeof(crgb) * CONFIG_HC_MATRIX_WIDTH * CONFIG_HC_MATRIX_HEIGHT);
        cv_blank(new_cv);

        struct am_window_data new_win = {
            .handle = NULL,
            .canvas = new_cv
        };
        win_data[i] = new_win;
        win_count++;
        ESP_LOGI(TAG, "    %s", app_info[i].name);
    }

    // Start first window task
    launch_window_task(app_info[win_idx], &win_data[win_idx]);

    while(1) {
        // Wait for notification
        uint32_t message;
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &message, portMAX_DELAY);
        
        if (message & AM_MSG_REFRESH) {
            // Redraw framebuffer
            redraw_fb(fb, win_data[win_idx].canvas);
            ESP_LOGI(TAG, "Redraw FB");
        } else if (message & AM_MSG_PREVAPP || message & AM_MSG_NEXTAPP) {
            // Switch apps
            if ((message & AM_MSG_PREVAPP && win_idx == 0) ||
                (message & AM_MSG_NEXTAPP && win_idx == (win_count - 1))) {
                ESP_LOGI(TAG, "Can't switch apps!");
                continue;
            }

            uint8_t prev_idx = win_idx;
            uint8_t l_idx, r_idx;
            if (message & AM_MSG_NEXTAPP) {
                l_idx = win_idx;
                r_idx = ++win_idx;
            } else {
                r_idx = win_idx;
                l_idx = --win_idx;
            }
            if (win_data[win_idx].handle == NULL) {
                ESP_LOGI(TAG, "Start %s task...", app_info[win_idx].name);
                launch_window_task(app_info[win_idx], &win_data[win_idx]);
            } else {
                ESP_LOGI(TAG, "Resume %s task...", app_info[win_idx].name);
                vTaskResume(win_data[win_idx].handle);
            }

            ESP_LOGI(TAG, "Playing animation...");
            play_slide_anim(fb,
                            win_data[l_idx].canvas,
                            win_data[r_idx].canvas,
                            (message & AM_MSG_NEXTAPP) != 0);

            ESP_LOGI(TAG, "Suspend %s task...", app_info[prev_idx].name);
            vTaskSuspend(win_data[prev_idx].handle);
        }
    }
}

void launch_am_task(struct am_params *params)
{
    if (am_handle != NULL)
    {
        ESP_LOGE(TAG, "AM task is already running!");
        return;
    }
    int ret = xTaskCreate(
        window_manager_task,
        "window_manager",
        AM_STACK_SIZE,
        (void *) params,
        AM_TASK_PRIORITY,
        &am_handle
    );
    if (ret != pdPASS)
        ESP_LOGE(TAG, "Failed to start AM task");
}

void am_send_msg(uint32_t message)
{
    xTaskNotify(am_handle, message, eSetBits);
}

void am_send_msg_from_isr(uint32_t message, BaseType_t *higher_task_wakeup)
{
    xTaskNotifyFromISR(am_handle, message, eSetBits, higher_task_wakeup);
}

void am_send_input_event(uint32_t event, BaseType_t *higher_task_wakeup)
{
    xTaskNotifyFromISR(win_data[win_idx].handle, event, eSetBits, higher_task_wakeup);
}
