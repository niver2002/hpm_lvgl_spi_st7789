/*
 * LVGL Demos Menu for HPM6E00 + ST7789 (SPI + DMA)
 *
 * This example is inspired by HPM SDK `samples/lvgl/common/lvgl.c`,
 * but uses a responsive layout that works on narrow screens such as 172x320.
 */

#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "hpm_lvgl_spi.h"

#include <lvgl.h>
#include <demos/lv_demos.h>

/* Some boards may not provide these helpers. Provide weak defaults so the demo can still build. */
ATTR_WEAK void board_init_lcd(void) {}

typedef struct demo_info {
    const char *name;
    void (*entry)(void);
} demo_info_t;

static demo_info_t demo_infos[] = {
#if LV_USE_DEMO_WIDGETS
    { "widgets", lv_demo_widgets },
#endif
#if LV_USE_DEMO_BENCHMARK
    { "benchmark", lv_demo_benchmark },
#endif
#if LV_USE_DEMO_STRESS
    { "stress", lv_demo_stress },
#endif
#if LV_USE_DEMO_FLEX_LAYOUT
    { "flex_layout", lv_demo_flex_layout },
#endif
#if LV_USE_DEMO_MUSIC
    { "music", lv_demo_music },
#endif
};

static lv_color_t def_bg_color;

static void demo_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code != LV_EVENT_CLICKED) {
        return;
    }

    demo_info_t *demo_info = lv_obj_get_user_data(btn);
    if ((demo_info == NULL) || (demo_info->entry == NULL)) {
        return;
    }

    lv_obj_clean(lv_screen_active());
    lv_obj_set_style_bg_color(lv_screen_active(), def_bg_color, LV_PART_MAIN);
    demo_info->entry();
}

static void create_menu(void)
{
    lv_obj_t *scr = lv_screen_active();
    int32_t screen_w = lv_obj_get_width(scr);
    int32_t screen_h = lv_obj_get_height(scr);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL demos");
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    /* Menu container */
    lv_obj_t *menu = lv_obj_create(scr);
    lv_obj_remove_style_all(menu);
    lv_obj_set_size(menu, lv_pct(100), lv_pct(78));
    lv_obj_align(menu, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_layout(menu, LV_LAYOUT_FLEX);

    /* Narrow screens: use a single column for readability */
    if (screen_w < 240) {
        lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    } else {
        lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    }

    lv_obj_set_style_pad_row(menu, 8, 0);
    lv_obj_set_style_pad_column(menu, 8, 0);
    lv_obj_set_style_pad_all(menu, 8, 0);

    /* Button style */
    static lv_style_t style_btn;
    static bool style_inited = false;
    if (!style_inited) {
        lv_style_init(&style_btn);
        lv_style_set_bg_opa(&style_btn, LV_OPA_TRANSP);
        lv_style_set_border_width(&style_btn, 2);
        lv_style_set_border_color(&style_btn, lv_color_hex(0xFFFFFF));
        lv_style_set_radius(&style_btn, LV_RADIUS_CIRCLE);
        style_inited = true;
    }

    uint32_t demo_count = (uint32_t)(sizeof(demo_infos) / sizeof(demo_infos[0]));
    uint32_t btn_h = (screen_h < 240) ? 40 : 48;
    if (screen_w < 240) {
        /* Portrait small screen: make buttons larger for touch */
        btn_h = 44;
    }

    for (uint32_t i = 0; i < demo_count; i++) {
        lv_obj_t *btn = lv_button_create(menu);
        lv_obj_set_user_data(btn, &demo_infos[i]);
        lv_obj_add_style(btn, &style_btn, 0);
        lv_obj_add_event_cb(btn, demo_btn_event_cb, LV_EVENT_ALL, NULL);

        if (screen_w < 240) {
            lv_obj_set_width(btn, lv_pct(92));
        } else {
            lv_obj_set_width(btn, lv_pct(45));
        }
        lv_obj_set_height(btn, btn_h);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, demo_infos[i].name);
        lv_obj_center(label);
    }
}

int main(void)
{
    board_init();
    board_init_lcd();

    /* Init LVGL + display (SPI DMA) */
    if (hpm_lvgl_spi_init() == NULL) {
        while (1) {
        }
    }

    def_bg_color = lv_obj_get_style_bg_color(lv_screen_active(), LV_PART_MAIN);

    create_menu();

    while (1) {
        lv_timer_handler();
        board_delay_us(1000);
    }
}

