#include "screen_sibilla.h"
#include "indicator_storage.h"
#include "app_config.h"
#include "lvgl/lvgl.h"

lv_obj_t *ui_screen_sibilla;

static lv_obj_t *s_header_lbl;

/* Legge srv_name da NVS e aggiorna il label header. */
static void refresh_header(void)
{
    char name[32];
    size_t len = sizeof(name);
    if (indicator_storage_read((char *)NVS_KEY_SERVER_NAME, name, &len) == 0 && len > 1) {
        name[sizeof(name) - 1] = '\0';
    } else {
        strncpy(name, APP_CFG_SERVER_NAME, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
    lv_label_set_text(s_header_lbl, name);
}

/* Aggiorna il nome al caricamento della schermata (hostname potrebbe essere
 * stato scritto in NVS da indicator_system dopo il primo boot fetch). */
static void on_screen_load_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOAD_START) return;
    refresh_header();
}

void screen_sibilla_init(void)
{
    ui_screen_sibilla = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_sibilla, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_sibilla, lv_color_hex(0x0d1b2a),
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    s_header_lbl = lv_label_create(ui_screen_sibilla);
    lv_obj_set_style_text_color(s_header_lbl, lv_color_white(),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_header_lbl, &lv_font_montserrat_20,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_header_lbl, LV_ALIGN_CENTER, 0, 0);

    /* Valore iniziale da NVS (o default) */
    refresh_header();

    /* Refresh al ritorno su questa schermata */
    lv_obj_add_event_cb(ui_screen_sibilla, on_screen_load_start,
                        LV_EVENT_SCREEN_LOAD_START, NULL);
}
