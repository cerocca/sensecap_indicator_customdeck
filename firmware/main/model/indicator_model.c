#include "indicator_model.h"
#include "indicator_storage.h"
#include "indicator_wifi.h"
#include "indicator_display.h"
#include "indicator_time.h"
#include "indicator_btn.h"
#include "indicator_city.h"
#include "indicator_config.h"
#include "indicator_hue.h"

int indicator_model_init(void)
{
    indicator_storage_init();
    indicator_sensor_init();
    indicator_wifi_init();
    indicator_config_init();   /* registra fetch config al primo IP_EVENT_STA_GOT_IP */
    indicator_hue_init();      /* registra poll Hue al primo IP_EVENT_STA_GOT_IP */
    indicator_time_init();
    indicator_city_init();
    indicator_display_init();  // lcd bl on
    indicator_btn_init();
}