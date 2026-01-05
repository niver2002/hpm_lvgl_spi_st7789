/*
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * TSN Dashboard Demo for HPM6E00 FULL_PORT
 * 
 * Features:
 * - 3-port TSN switch status display
 * - Smooth menu navigation with button control
 * - Real-time FPS display
 * - Animation demo
 */

#include <stdio.h>
#include "board.h"
#include "hpm_gpio_drv.h"
#include "hpm_clock_drv.h"
#include "hpm_lvgl_spi.h"

/*============================================================================
 * Configuration
 *============================================================================*/

/* Menu pages */
typedef enum {
    PAGE_OVERVIEW = 0,
    PAGE_PORT1,
    PAGE_PORT2,
    PAGE_PORT3,
    PAGE_SETTINGS,
    PAGE_COUNT
} page_id_t;

/* Port status */
typedef struct {
    bool link_up;
    uint32_t speed_mbps;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t errors;
} port_status_t;

/*============================================================================
 * UI Objects
 *============================================================================*/

static struct {
    /* Current page */
    page_id_t current_page;
    
    /* Main container */
    lv_obj_t *screen;
    
    /* Title bar */
    lv_obj_t *title_label;
    lv_obj_t *fps_label;
    
    /* Content area */
    lv_obj_t *content;
    
    /* Page-specific objects */
    lv_obj_t *port_indicators[3];
    lv_obj_t *speed_bars[3];
    lv_obj_t *stat_labels[3];
    
    /* Navigation dots */
    lv_obj_t *nav_dots[PAGE_COUNT];
    
    /* Simulated port data */
    port_status_t ports[3];
    
    /* Animation */
    uint32_t anim_counter;
} ui;

/*============================================================================
 * Button handling
 *============================================================================*/

static bool key_pressed[4] = {false};
static uint32_t key_debounce[4] = {0};

#define DEBOUNCE_MS 50

static bool is_key_pressed(uint8_t key_idx)
{
    bool pressed = false;
    GPIO_Type *gpio = NULL;
    uint32_t index = 0;
    uint32_t pin = 0;
    
    switch (key_idx) {
    case 0: /* KEY A */
        gpio = BOARD_KEYA_GPIO_CTRL;
        index = BOARD_KEYA_GPIO_INDEX;
        pin = BOARD_KEYA_GPIO_PIN;
        break;
    case 1: /* KEY B */
        gpio = BOARD_KEYB_GPIO_CTRL;
        index = BOARD_KEYB_GPIO_INDEX;
        pin = BOARD_KEYB_GPIO_PIN;
        break;
    case 2: /* KEY C */
        gpio = BOARD_KEYC_GPIO_CTRL;
        index = BOARD_KEYC_GPIO_INDEX;
        pin = BOARD_KEYC_GPIO_PIN;
        break;
    case 3: /* KEY D */
        gpio = BOARD_KEYD_GPIO_CTRL;
        index = BOARD_KEYD_GPIO_INDEX;
        pin = BOARD_KEYD_GPIO_PIN;
        break;
    default:
        return false;
    }
    
    pressed = (gpio_read_pin(gpio, index, pin) == 0);
    return pressed;
}

static bool key_just_pressed(uint8_t key_idx)
{
    bool pressed = is_key_pressed(key_idx);
    uint32_t now = hpm_lvgl_spi_tick_get();
    
    if (pressed && !key_pressed[key_idx]) {
        if (now - key_debounce[key_idx] > DEBOUNCE_MS) {
            key_pressed[key_idx] = true;
            key_debounce[key_idx] = now;
            return true;
        }
    } else if (!pressed) {
        key_pressed[key_idx] = false;
    }
    
    return false;
}

/*============================================================================
 * UI Creation
 *============================================================================*/

/* Colors */
#define COLOR_BG        lv_color_hex(0x1a1a2e)
#define COLOR_PANEL     lv_color_hex(0x16213e)
#define COLOR_ACCENT    lv_color_hex(0x0f4c75)
#define COLOR_GREEN     lv_color_hex(0x00ff88)
#define COLOR_RED       lv_color_hex(0xff4444)
#define COLOR_YELLOW    lv_color_hex(0xffcc00)
#define COLOR_TEXT      lv_color_hex(0xeaeaea)
#define COLOR_DIM       lv_color_hex(0x888888)

static void create_title_bar(void)
{
    /* Title */
    ui.title_label = lv_label_create(ui.screen);
    lv_obj_set_style_text_color(ui.title_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ui.title_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ui.title_label, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_label_set_text(ui.title_label, "TSN SWITCH");
    
    /* FPS counter */
    ui.fps_label = lv_label_create(ui.screen);
    lv_obj_set_style_text_color(ui.fps_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(ui.fps_label, &lv_font_montserrat_12, 0);
    lv_obj_align(ui.fps_label, LV_ALIGN_TOP_RIGHT, -8, 10);
    lv_label_set_text(ui.fps_label, "-- FPS");
}

static void create_nav_dots(void)
{
    lv_obj_t *nav_cont = lv_obj_create(ui.screen);
    lv_obj_remove_style_all(nav_cont);
    lv_obj_set_size(nav_cont, 80, 12);
    lv_obj_align(nav_cont, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_flex_flow(nav_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(nav_cont, 6, 0);
    
    for (int i = 0; i < PAGE_COUNT; i++) {
        ui.nav_dots[i] = lv_obj_create(nav_cont);
        lv_obj_remove_style_all(ui.nav_dots[i]);
        lv_obj_set_size(ui.nav_dots[i], 8, 8);
        lv_obj_set_style_radius(ui.nav_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ui.nav_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ui.nav_dots[i], COLOR_DIM, 0);
    }
}

static void update_nav_dots(void)
{
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (i == ui.current_page) {
            lv_obj_set_style_bg_color(ui.nav_dots[i], COLOR_ACCENT, 0);
            lv_obj_set_size(ui.nav_dots[i], 16, 8);
        } else {
            lv_obj_set_style_bg_color(ui.nav_dots[i], COLOR_DIM, 0);
            lv_obj_set_size(ui.nav_dots[i], 8, 8);
        }
    }
}

static lv_obj_t *create_port_card(lv_obj_t *parent, int port_num)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 150, 80);
    lv_obj_set_style_bg_color(card, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    
    /* Port number */
    lv_obj_t *port_label = lv_label_create(card);
    lv_obj_set_style_text_color(port_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(port_label, &lv_font_montserrat_14, 0);
    lv_label_set_text_fmt(port_label, "PORT %d", port_num);
    lv_obj_align(port_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    /* Status indicator */
    ui.port_indicators[port_num - 1] = lv_obj_create(card);
    lv_obj_remove_style_all(ui.port_indicators[port_num - 1]);
    lv_obj_set_size(ui.port_indicators[port_num - 1], 12, 12);
    lv_obj_set_style_radius(ui.port_indicators[port_num - 1], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ui.port_indicators[port_num - 1], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui.port_indicators[port_num - 1], COLOR_GREEN, 0);
    lv_obj_align(ui.port_indicators[port_num - 1], LV_ALIGN_TOP_RIGHT, 0, 2);
    
    /* Speed bar */
    ui.speed_bars[port_num - 1] = lv_bar_create(card);
    lv_obj_set_size(ui.speed_bars[port_num - 1], 130, 8);
    lv_obj_align(ui.speed_bars[port_num - 1], LV_ALIGN_BOTTOM_LEFT, 0, -18);
    lv_bar_set_range(ui.speed_bars[port_num - 1], 0, 100);
    lv_bar_set_value(ui.speed_bars[port_num - 1], 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.speed_bars[port_num - 1], COLOR_BG, 0);
    lv_obj_set_style_bg_color(ui.speed_bars[port_num - 1], COLOR_ACCENT, LV_PART_INDICATOR);
    
    /* Stats label */
    ui.stat_labels[port_num - 1] = lv_label_create(card);
    lv_obj_set_style_text_color(ui.stat_labels[port_num - 1], COLOR_DIM, 0);
    lv_obj_set_style_text_font(ui.stat_labels[port_num - 1], &lv_font_montserrat_10, 0);
    lv_label_set_text(ui.stat_labels[port_num - 1], "1000 Mbps");
    lv_obj_align(ui.stat_labels[port_num - 1], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    return card;
}

/*============================================================================
 * Page content creation
 *============================================================================*/

static void create_overview_page(void)
{
    if (ui.content) {
        lv_obj_del(ui.content);
    }
    
    ui.content = lv_obj_create(ui.screen);
    lv_obj_remove_style_all(ui.content);
    lv_obj_set_size(ui.content, HPM_LVGL_LCD_WIDTH, HPM_LVGL_LCD_HEIGHT - 60);
    lv_obj_align(ui.content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(ui.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(ui.content, 8, 0);
    lv_obj_set_style_pad_top(ui.content, 8, 0);
    
    /* Create port cards */
    for (int i = 1; i <= 3; i++) {
        create_port_card(ui.content, i);
    }
    
    lv_label_set_text(ui.title_label, "TSN OVERVIEW");
}

static void create_port_detail_page(int port_num)
{
    if (ui.content) {
        lv_obj_del(ui.content);
    }
    
    ui.content = lv_obj_create(ui.screen);
    lv_obj_remove_style_all(ui.content);
    lv_obj_set_size(ui.content, HPM_LVGL_LCD_WIDTH - 16, HPM_LVGL_LCD_HEIGHT - 60);
    lv_obj_align(ui.content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_bg_color(ui.content, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(ui.content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui.content, 8, 0);
    lv_obj_set_style_pad_all(ui.content, 12, 0);
    
    port_status_t *port = &ui.ports[port_num - 1];
    
    /* Port title */
    lv_obj_t *title = lv_label_create(ui.content);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_label_set_text_fmt(title, "PORT %d DETAILS", port_num);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    
    /* Status */
    lv_obj_t *status = lv_label_create(ui.content);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    if (port->link_up) {
        lv_obj_set_style_text_color(status, COLOR_GREEN, 0);
        lv_label_set_text(status, "● LINK UP");
    } else {
        lv_obj_set_style_text_color(status, COLOR_RED, 0);
        lv_label_set_text(status, "● LINK DOWN");
    }
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 30);
    
    /* Speed */
    lv_obj_t *speed = lv_label_create(ui.content);
    lv_obj_set_style_text_color(speed, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(speed, &lv_font_montserrat_24, 0);
    lv_label_set_text_fmt(speed, "%d Mbps", port->speed_mbps);
    lv_obj_align(speed, LV_ALIGN_TOP_MID, 0, 60);
    
    /* Stats */
    lv_obj_t *rx_label = lv_label_create(ui.content);
    lv_obj_set_style_text_color(rx_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(rx_label, &lv_font_montserrat_12, 0);
    lv_label_set_text_fmt(rx_label, "RX: %lu packets", port->rx_packets);
    lv_obj_align(rx_label, LV_ALIGN_TOP_LEFT, 0, 110);
    
    lv_obj_t *tx_label = lv_label_create(ui.content);
    lv_obj_set_style_text_color(tx_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(tx_label, &lv_font_montserrat_12, 0);
    lv_label_set_text_fmt(tx_label, "TX: %lu packets", port->tx_packets);
    lv_obj_align(tx_label, LV_ALIGN_TOP_LEFT, 0, 130);
    
    lv_obj_t *err_label = lv_label_create(ui.content);
    lv_obj_set_style_text_color(err_label, port->errors > 0 ? COLOR_RED : COLOR_DIM, 0);
    lv_obj_set_style_text_font(err_label, &lv_font_montserrat_12, 0);
    lv_label_set_text_fmt(err_label, "Errors: %lu", port->errors);
    lv_obj_align(err_label, LV_ALIGN_TOP_LEFT, 0, 150);
    
    /* Animated arc */
    lv_obj_t *arc = lv_arc_create(ui.content);
    lv_obj_set_size(arc, 80, 80);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_value(arc, (port->rx_packets + port->tx_packets) % 100);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 8, 0);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_align(arc, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    lv_label_set_text_fmt(ui.title_label, "PORT %d", port_num);
}

static void create_settings_page(void)
{
    if (ui.content) {
        lv_obj_del(ui.content);
    }
    
    ui.content = lv_obj_create(ui.screen);
    lv_obj_remove_style_all(ui.content);
    lv_obj_set_size(ui.content, HPM_LVGL_LCD_WIDTH - 16, HPM_LVGL_LCD_HEIGHT - 60);
    lv_obj_align(ui.content, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_bg_color(ui.content, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(ui.content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui.content, 8, 0);
    lv_obj_set_style_pad_all(ui.content, 12, 0);
    
    /* Settings title */
    lv_obj_t *title = lv_label_create(ui.content);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    
    /* Info */
    lv_obj_t *info1 = lv_label_create(ui.content);
    lv_obj_set_style_text_color(info1, COLOR_DIM, 0);
    lv_obj_set_style_text_font(info1, &lv_font_montserrat_12, 0);
    lv_label_set_text(info1, "HPM6E00 FULL PORT");
    lv_obj_align(info1, LV_ALIGN_TOP_LEFT, 0, 40);
    
    lv_obj_t *info2 = lv_label_create(ui.content);
    lv_obj_set_style_text_color(info2, COLOR_DIM, 0);
    lv_obj_set_style_text_font(info2, &lv_font_montserrat_12, 0);
    lv_label_set_text(info2, "TSN 3-Port Switch");
    lv_obj_align(info2, LV_ALIGN_TOP_LEFT, 0, 60);
    
    lv_obj_t *info3 = lv_label_create(ui.content);
    lv_obj_set_style_text_color(info3, COLOR_DIM, 0);
    lv_obj_set_style_text_font(info3, &lv_font_montserrat_12, 0);
    lv_label_set_text(info3, "Display: ST7789 SPI");
    lv_obj_align(info3, LV_ALIGN_TOP_LEFT, 0, 80);
    
    lv_obj_t *info4 = lv_label_create(ui.content);
    lv_obj_set_style_text_color(info4, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(info4, &lv_font_montserrat_12, 0);
    lv_label_set_text(info4, "SPI: 40MHz + DMA");
    lv_obj_align(info4, LV_ALIGN_TOP_LEFT, 0, 100);
    
    /* Controls */
    lv_obj_t *ctrl = lv_label_create(ui.content);
    lv_obj_set_style_text_color(ctrl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ctrl, &lv_font_montserrat_12, 0);
    lv_label_set_text(ctrl, "KEY A/B: Navigate\nKEY C: Select\nKEY D: Back");
    lv_obj_align(ctrl, LV_ALIGN_BOTTOM_LEFT, 0, -10);
    
    lv_label_set_text(ui.title_label, "SETTINGS");
}

/*============================================================================
 * Page navigation
 *============================================================================*/

static void switch_to_page(page_id_t page)
{
    ui.current_page = page;
    
    switch (page) {
    case PAGE_OVERVIEW:
        create_overview_page();
        break;
    case PAGE_PORT1:
        create_port_detail_page(1);
        break;
    case PAGE_PORT2:
        create_port_detail_page(2);
        break;
    case PAGE_PORT3:
        create_port_detail_page(3);
        break;
    case PAGE_SETTINGS:
        create_settings_page();
        break;
    default:
        break;
    }
    
    update_nav_dots();
}

static void next_page(void)
{
    page_id_t next = (ui.current_page + 1) % PAGE_COUNT;
    switch_to_page(next);
}

static void prev_page(void)
{
    page_id_t prev = (ui.current_page + PAGE_COUNT - 1) % PAGE_COUNT;
    switch_to_page(prev);
}

/*============================================================================
 * Data simulation
 *============================================================================*/

static void init_port_data(void)
{
    /* Simulate port data */
    ui.ports[0].link_up = true;
    ui.ports[0].speed_mbps = 1000;
    ui.ports[0].rx_packets = 123456;
    ui.ports[0].tx_packets = 98765;
    ui.ports[0].errors = 0;
    
    ui.ports[1].link_up = true;
    ui.ports[1].speed_mbps = 1000;
    ui.ports[1].rx_packets = 87654;
    ui.ports[1].tx_packets = 76543;
    ui.ports[1].errors = 2;
    
    ui.ports[2].link_up = false;
    ui.ports[2].speed_mbps = 0;
    ui.ports[2].rx_packets = 0;
    ui.ports[2].tx_packets = 0;
    ui.ports[2].errors = 0;
}

static void update_port_data(void)
{
    /* Simulate changing data */
    ui.anim_counter++;
    
    for (int i = 0; i < 3; i++) {
        if (ui.ports[i].link_up) {
            ui.ports[i].rx_packets += 100 + (ui.anim_counter % 50);
            ui.ports[i].tx_packets += 80 + (ui.anim_counter % 40);
        }
    }
    
    /* Update overview page if visible */
    if (ui.current_page == PAGE_OVERVIEW) {
        for (int i = 0; i < 3; i++) {
            if (ui.port_indicators[i]) {
                lv_obj_set_style_bg_color(ui.port_indicators[i], 
                    ui.ports[i].link_up ? COLOR_GREEN : COLOR_RED, 0);
            }
            
            if (ui.speed_bars[i]) {
                int val = ui.ports[i].link_up ? 
                    (50 + ((ui.anim_counter + i * 20) % 50)) : 0;
                lv_bar_set_value(ui.speed_bars[i], val, LV_ANIM_ON);
            }
            
            if (ui.stat_labels[i]) {
                lv_label_set_text_fmt(ui.stat_labels[i], "%d Mbps", 
                    ui.ports[i].speed_mbps);
            }
        }
    }
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    /* Board initialization */
    board_init();
    board_init_key();   /* Initialize KEY GPIO */
    board_init_lcd();   /* Initialize LCD pins and GPIO */
    
    printf("TSN Dashboard Demo\n");
    printf("Screen: 172x320 ST7789\n");
    printf("SPI: 40MHz with DMA + Partial Refresh\n");
    
    /* Initialize LVGL with SPI display */
    if (hpm_lvgl_spi_init() == NULL) {
        printf("Failed to initialize display!\n");
        while (1) {
        }
    }
    printf("Display initialized\n");
    
    /* Get screen */
    ui.screen = lv_screen_active();
    lv_obj_set_style_bg_color(ui.screen, COLOR_BG, 0);
    
    /* Initialize port data */
    init_port_data();
    
    /* Create UI */
    create_title_bar();
    create_nav_dots();
    
    /* Show overview page */
    switch_to_page(PAGE_OVERVIEW);
    
    printf("UI ready. Use buttons to navigate.\n");
    printf("KEY A: Previous, KEY B: Next\n");
    
    uint32_t last_update = 0;
    uint32_t last_fps_update = 0;
    
    /* Main loop */
    while (1) {
        uint32_t now = hpm_lvgl_spi_tick_get();
        
        /* Handle button input */
        if (key_just_pressed(0)) {  /* KEY A - Previous */
            prev_page();
        }
        if (key_just_pressed(1)) {  /* KEY B - Next */
            next_page();
        }
        if (key_just_pressed(2)) {  /* KEY C - Select/Action */
            /* Could add action here */
        }
        if (key_just_pressed(3)) {  /* KEY D - Back to overview */
            if (ui.current_page != PAGE_OVERVIEW) {
                switch_to_page(PAGE_OVERVIEW);
            }
        }
        
        /* Update data periodically */
        if (now - last_update > 100) {
            update_port_data();
            last_update = now;
        }
        
        /* Update FPS display */
        if (now - last_fps_update > 500) {
            uint32_t fps = hpm_lvgl_spi_get_fps();
            if (ui.fps_label) {
                lv_label_set_text_fmt(ui.fps_label, "%lu FPS", fps);
            }
            last_fps_update = now;
        }
        
        /* Run LVGL tasks */
        lv_timer_handler();
        
        /* Small delay to prevent busy loop */
        board_delay_us(1000);  /* 1ms */
    }
    
    return 0;
}
