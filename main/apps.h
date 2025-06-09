#ifndef __APPS_H__
#define __APPS_H__

#define APP_INFO(app_name)  { .name = #app_name, .ui_task = app_name ## _ui_task }

#include "app_manager.h"

/* App headers go here */
#include "sin_wave.h"
#include "clock_app.h"
#include "wifi_status_app.h"

static struct am_app_info registered_apps[] = {
    APP_INFO(wifi_status),
    APP_INFO(clock),
    APP_INFO(sin_wave),
    { /* sentinel */}
};

#endif
