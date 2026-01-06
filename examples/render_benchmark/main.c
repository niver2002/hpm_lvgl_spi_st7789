/*
 * LVGL Render Benchmark Demo for HPM6E00 + ST7789/GC9307 (SPI + DMA)
 *
 * Goal:
 * - Generate different invalidation patterns (scatter / stripe / full refresh)
 * - Display live flush stats (flush/s, KB/s, last area)
 *
 * Keys (HPM6E00 FULL_PORT):
 * - KEY A: previous mode
 * - KEY B: next mode
 * - KEY C: pause/resume animation
 * - KEY D: reset statistics
 */

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "hpm_gpio_drv.h"
#include "hpm_lvgl_spi.h"

/* Some boards (e.g. hpm6e00evk in hpm_sdk) may not provide these helpers.
 * Provide weak defaults so the demo can still build. */
ATTR_WEAK void board_init_key(void) {}
ATTR_WEAK void board_init_lcd(void) {}

/*============================================================================
 * Button handling (same mapping as tsn_dashboard)
 *============================================================================*/

static bool key_pressed[4] = {false};
static uint32_t key_debounce[4] = {0};

#define DEBOUNCE_MS 50

#if defined(BOARD_KEYA_GPIO_CTRL) && defined(BOARD_KEYA_GPIO_INDEX) && defined(BOARD_KEYA_GPIO_PIN) && \
    defined(BOARD_KEYB_GPIO_CTRL) && defined(BOARD_KEYB_GPIO_INDEX) && defined(BOARD_KEYB_GPIO_PIN) && \
    defined(BOARD_KEYC_GPIO_CTRL) && defined(BOARD_KEYC_GPIO_INDEX) && defined(BOARD_KEYC_GPIO_PIN) && \
    defined(BOARD_KEYD_GPIO_CTRL) && defined(BOARD_KEYD_GPIO_INDEX) && defined(BOARD_KEYD_GPIO_PIN)
#define DEMO_HAS_KEYS 1
#else
#define DEMO_HAS_KEYS 0
#endif

static bool is_key_pressed(uint8_t key_idx)
{
#if DEMO_HAS_KEYS
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

    return (gpio_read_pin(gpio, index, pin) == 0);
#else
    (void)key_idx;
    return false;
#endif
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
 * Benchmark UI
 *============================================================================*/

typedef enum {
    BENCH_MODE_SCATTER = 0,
    BENCH_MODE_STRIPE,
    BENCH_MODE_FULL,
    BENCH_MODE_COUNT,
} bench_mode_t;

#define ANIM_PERIOD_MS  16
#define STATS_PERIOD_MS 250

#define DOT_COUNT  24
#define DOT_SIZE   8

/* Colors */
#define COLOR_BG        lv_color_hex(0x0b1020)
#define COLOR_TEXT      lv_color_hex(0xeaeaea)
#define COLOR_DIM       lv_color_hex(0x9aa3b2)
#define COLOR_ACCENT    lv_color_hex(0x3b82f6)
#define COLOR_WARN      lv_color_hex(0xf59e0b)

static struct {
    bench_mode_t mode;
    bool paused;

    lv_obj_t *screen;
    lv_obj_t *title_label;
    lv_obj_t *stats_label;
    lv_obj_t *help_label;
    lv_obj_t *content;

    /* Scatter */
    lv_obj_t *dots[DOT_COUNT];
    int16_t dot_x[DOT_COUNT];
    int16_t dot_y[DOT_COUNT];
    int8_t dot_vx[DOT_COUNT];
    int8_t dot_vy[DOT_COUNT];

    /* Stripe */
    lv_obj_t *stripe;
    int16_t stripe_x;
    int8_t stripe_vx;

    /* Full refresh */
    lv_obj_t *full_bg;
    uint32_t full_color_step;

    /* Stats baseline */
    uint32_t last_stats_ms;
    uint32_t last_flush_count;
    uint64_t last_flush_bytes;

    lv_timer_t *anim_timer;
} bench;

static const char *bench_mode_name(bench_mode_t mode)
{
    switch (mode) {
    case BENCH_MODE_SCATTER:
        return "SCATTER";
    case BENCH_MODE_STRIPE:
        return "STRIPE";
    case BENCH_MODE_FULL:
        return "FULL";
    default:
        return "UNKNOWN";
    }
}

static void ui_update_title(void)
{
    lv_label_set_text_fmt(bench.title_label, "LVGL BENCH  %s%s",
                          bench_mode_name(bench.mode),
                          bench.paused ? "  (PAUSE)" : "");
}

static void bench_reset_stats(void)
{
    hpm_lvgl_spi_reset_stats();

    hpm_lvgl_spi_stats_t s;
    hpm_lvgl_spi_get_stats(&s);

    bench.last_stats_ms = hpm_lvgl_spi_tick_get();
    bench.last_flush_count = s.flush_count;
    bench.last_flush_bytes = s.flush_bytes;
}

static void bench_build_scatter(void)
{
    int16_t w = (int16_t)lv_obj_get_width(bench.content);
    int16_t h = (int16_t)lv_obj_get_height(bench.content);

    for (uint32_t i = 0; i < DOT_COUNT; i++) {
        lv_obj_t *dot = lv_obj_create(bench.content);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

        /* Color cycle */
        uint32_t c = 0x22ccff;
        if ((i % 3) == 1) {
            c = 0x22ff88;
        } else if ((i % 3) == 2) {
            c = 0xff4477;
        }
        lv_obj_set_style_bg_color(dot, lv_color_hex(c), 0);

        /* Deterministic pseudo-random init */
        int16_t x = (int16_t)((i * 37U) % (uint32_t)(w - DOT_SIZE));
        int16_t y = (int16_t)((i * 61U) % (uint32_t)(h - DOT_SIZE));
        int8_t vx = (int8_t)((i % 3) + 1);
        int8_t vy = (int8_t)(((i + 1) % 3) + 1);
        if (i & 0x1U) {
            vx = -vx;
        }
        if (i & 0x2U) {
            vy = -vy;
        }

        bench.dots[i] = dot;
        bench.dot_x[i] = x;
        bench.dot_y[i] = y;
        bench.dot_vx[i] = vx;
        bench.dot_vy[i] = vy;

        lv_obj_set_pos(dot, x, y);
    }
}

static void bench_build_stripe(void)
{
    int16_t w = (int16_t)lv_obj_get_width(bench.content);
    int16_t h = (int16_t)lv_obj_get_height(bench.content);

    bench.stripe = lv_obj_create(bench.content);
    lv_obj_remove_style_all(bench.stripe);
    lv_obj_set_size(bench.stripe, 22, h);
    lv_obj_set_style_bg_opa(bench.stripe, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bench.stripe, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(bench.stripe, 6, 0);

    bench.stripe_x = 0;
    bench.stripe_vx = 3;
    lv_obj_set_pos(bench.stripe, bench.stripe_x, 0);

    /* Add a few static widgets to mimic dashboard workload */
    lv_obj_t *lbl = lv_label_create(bench.content);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 6, 6);
    lv_label_set_text(lbl, "Stripe moves (partial)");

    lv_obj_t *bar = lv_bar_create(bench.content);
    lv_obj_set_size(bar, w - 12, 10);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 75, LV_ANIM_OFF);
}

static void bench_build_full(void)
{
    int16_t w = (int16_t)lv_obj_get_width(bench.content);
    int16_t h = (int16_t)lv_obj_get_height(bench.content);

    bench.full_bg = lv_obj_create(bench.content);
    lv_obj_remove_style_all(bench.full_bg);
    lv_obj_set_size(bench.full_bg, w, h);
    lv_obj_set_style_bg_opa(bench.full_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bench.full_bg, lv_color_hex(0x112233), 0);

    bench.full_color_step = 0;

    lv_obj_t *lbl = lv_label_create(bench.full_bg);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);
    lv_label_set_text(lbl, "Full-area redraw");
}

static void bench_set_mode(bench_mode_t mode)
{
    bench.mode = mode;
    bench.paused = false;

    lv_obj_clean(bench.content);
    memset(bench.dots, 0, sizeof(bench.dots));
    bench.stripe = NULL;
    bench.full_bg = NULL;

    switch (bench.mode) {
    case BENCH_MODE_SCATTER:
        bench_build_scatter();
        break;
    case BENCH_MODE_STRIPE:
        bench_build_stripe();
        break;
    case BENCH_MODE_FULL:
        bench_build_full();
        break;
    default:
        break;
    }

    ui_update_title();
    bench_reset_stats();
}

static void ui_update_stats(void)
{
    uint32_t now = hpm_lvgl_spi_tick_get();
    if (now - bench.last_stats_ms < STATS_PERIOD_MS) {
        return;
    }

    hpm_lvgl_spi_stats_t s;
    hpm_lvgl_spi_get_stats(&s);

    uint32_t dt_ms = now - bench.last_stats_ms;
    uint32_t df = s.flush_count - bench.last_flush_count;
    uint64_t db = s.flush_bytes - bench.last_flush_bytes;

    uint32_t flush_ps = (dt_ms > 0) ? (df * 1000U) / dt_ms : 0;
    uint32_t kb_ps = (dt_ms > 0) ? (uint32_t)((db * 1000ULL) / (uint64_t)dt_ms / 1024ULL) : 0;

    int32_t last_w = (int32_t)s.last_flush_area.x2 - (int32_t)s.last_flush_area.x1 + 1;
    int32_t last_h = (int32_t)s.last_flush_area.y2 - (int32_t)s.last_flush_area.y1 + 1;
    if (s.flush_count == 0 || last_w < 0 || last_h < 0) {
        last_w = 0;
        last_h = 0;
    }

    lv_label_set_text_fmt(bench.stats_label,
                          "Flush %lu/s  %lu KB/s\nLast %ldx%ld  Buf %d",
                          (unsigned long)flush_ps,
                          (unsigned long)kb_ps,
                          (long)last_w,
                          (long)last_h,
                          (int)HPM_LVGL_FB_LINES);

    bench.last_stats_ms = now;
    bench.last_flush_count = s.flush_count;
    bench.last_flush_bytes = s.flush_bytes;
}

static void anim_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (bench.paused) {
        return;
    }

    int16_t w = (int16_t)lv_obj_get_width(bench.content);
    int16_t h = (int16_t)lv_obj_get_height(bench.content);

    switch (bench.mode) {
    case BENCH_MODE_SCATTER:
        for (uint32_t i = 0; i < DOT_COUNT; i++) {
            int16_t x = (int16_t)(bench.dot_x[i] + bench.dot_vx[i]);
            int16_t y = (int16_t)(bench.dot_y[i] + bench.dot_vy[i]);

            if (x < 0) {
                x = 0;
                bench.dot_vx[i] = (int8_t)-bench.dot_vx[i];
            } else if (x > (w - DOT_SIZE)) {
                x = (int16_t)(w - DOT_SIZE);
                bench.dot_vx[i] = (int8_t)-bench.dot_vx[i];
            }

            if (y < 0) {
                y = 0;
                bench.dot_vy[i] = (int8_t)-bench.dot_vy[i];
            } else if (y > (h - DOT_SIZE)) {
                y = (int16_t)(h - DOT_SIZE);
                bench.dot_vy[i] = (int8_t)-bench.dot_vy[i];
            }

            bench.dot_x[i] = x;
            bench.dot_y[i] = y;

            if (bench.dots[i]) {
                lv_obj_set_pos(bench.dots[i], x, y);
            }
        }
        break;
    case BENCH_MODE_STRIPE: {
        bench.stripe_x = (int16_t)(bench.stripe_x + bench.stripe_vx);
        if (bench.stripe_x < 0) {
            bench.stripe_x = 0;
            bench.stripe_vx = (int8_t)-bench.stripe_vx;
        } else if (bench.stripe_x > (w - 22)) {
            bench.stripe_x = (int16_t)(w - 22);
            bench.stripe_vx = (int8_t)-bench.stripe_vx;
        }

        if (bench.stripe) {
            lv_obj_set_x(bench.stripe, bench.stripe_x);
        }
        break;
    }
    case BENCH_MODE_FULL: {
        bench.full_color_step++;
        uint32_t r = (bench.full_color_step * 5U) & 0xFFU;
        uint32_t g = (bench.full_color_step * 3U) & 0xFFU;
        uint32_t b = (bench.full_color_step * 7U) & 0xFFU;
        uint32_t rgb = (r << 16) | (g << 8) | b;

        if (bench.full_bg) {
            lv_obj_set_style_bg_color(bench.full_bg, lv_color_hex(rgb), 0);
        }
        break;
    }
    default:
        break;
    }
}

static void ui_create(void)
{
    bench.screen = lv_screen_active();
    lv_obj_set_style_bg_color(bench.screen, COLOR_BG, 0);

    bench.title_label = lv_label_create(bench.screen);
    lv_obj_set_style_text_color(bench.title_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(bench.title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(bench.title_label, LV_ALIGN_TOP_LEFT, 6, 6);

    bench.stats_label = lv_label_create(bench.screen);
    lv_obj_set_style_text_color(bench.stats_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(bench.stats_label, &lv_font_montserrat_12, 0);
    lv_obj_align(bench.stats_label, LV_ALIGN_TOP_LEFT, 6, 26);
    lv_label_set_text(bench.stats_label, "Flush --/s  -- KB/s\nLast --x--  Buf --");

    bench.help_label = lv_label_create(bench.screen);
    lv_obj_set_style_text_color(bench.help_label, COLOR_WARN, 0);
    lv_obj_set_style_text_font(bench.help_label, &lv_font_montserrat_10, 0);
    lv_obj_align(bench.help_label, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_label_set_text(bench.help_label, DEMO_HAS_KEYS ? "A/B Mode  C Pause  D Reset" : "No keys (board.h has no KEY macros)");

    bench.content = lv_obj_create(bench.screen);
    lv_obj_remove_style_all(bench.content);
    lv_obj_set_pos(bench.content, 0, 56);
    lv_obj_set_size(bench.content, HPM_LVGL_LCD_WIDTH, (HPM_LVGL_LCD_HEIGHT - 56 - 20));
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    board_init();
    board_init_key();
    board_init_lcd();

    printf("LVGL Render Benchmark Demo\n");
    printf("Screen: %dx%d, SPI: %lu Hz\n", (int)HPM_LVGL_LCD_WIDTH, (int)HPM_LVGL_LCD_HEIGHT,
           (unsigned long)HPM_LVGL_SPI_FREQ);

    if (hpm_lvgl_spi_init() == NULL) {
        printf("Failed to initialize display!\n");
        while (1) {
        }
    }

    memset(&bench, 0, sizeof(bench));

    ui_create();

    bench.anim_timer = lv_timer_create(anim_timer_cb, ANIM_PERIOD_MS, NULL);
    (void)bench.anim_timer;

    bench_set_mode(BENCH_MODE_SCATTER);

    while (1) {
        if (key_just_pressed(0)) { /* KEY A */
            bench_set_mode((bench_mode_t)((bench.mode + BENCH_MODE_COUNT - 1) % BENCH_MODE_COUNT));
        }
        if (key_just_pressed(1)) { /* KEY B */
            bench_set_mode((bench_mode_t)((bench.mode + 1) % BENCH_MODE_COUNT));
        }
        if (key_just_pressed(2)) { /* KEY C */
            bench.paused = !bench.paused;
            ui_update_title();
            bench_reset_stats();
        }
        if (key_just_pressed(3)) { /* KEY D */
            bench_reset_stats();
        }

        ui_update_stats();

        lv_timer_handler();
        board_delay_us(1000);
    }

    return 0;
}
