#include "screen_ai.h"
#include "lvgl/lvgl.h"

lv_obj_t *ui_screen_ai;

void screen_ai_init(void)
{
    ui_screen_ai = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_screen_ai, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_screen_ai, lv_color_hex(0x12122b), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label = lv_label_create(ui_screen_ai);
    lv_label_set_text(label, "AI");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}
