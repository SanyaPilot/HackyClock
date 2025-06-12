#ifndef __APP_MANAGER_H__
#define __APP_MANAGER_H__

#include "freertos/FreeRTOS.h"

// Message bits
#define AM_MSG_REFRESH  0x1
#define AM_MSG_PREVAPP  0x2
#define AM_MSG_NEXTAPP  0x4

// Input events
#define EVENT_BTN_CLICK     0x1
#define EVENT_KNOB_RIGHT    0x2
#define EVENT_KNOB_LEFT     0x4

struct am_app_info
{
    const char *name;
    TaskFunction_t ui_task;
};

struct am_params
{
    struct framebuffer *framebuffer;
    struct am_app_info *apps;
};

void launch_am_task(struct am_params *params);
void am_send_msg(uint32_t message);
void am_send_msg_from_isr(uint32_t message, BaseType_t *higher_task_wakeup);
void am_send_input_event(uint32_t event, BaseType_t *higher_task_wakeup);
#endif
