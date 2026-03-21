#include "screen_hue.h"
#include "lvgl/lvgl.h"

lv_obj_t *ui_screen_hue;

void screen_hue_init(void)
{
    ui_screen_hue = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_hue, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_hue, lv_color_hex(0x1a1a2e), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label = lv_label_create(ui_screen_hue);
    lv_label_set_text(label, "Hue Control");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}
