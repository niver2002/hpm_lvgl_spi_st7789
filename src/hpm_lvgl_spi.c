/*
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * LVGL SPI Display Adapter Implementation
 */

#include "hpm_lvgl_spi.h"
#include "board.h"
#include "hpm_clock_drv.h"
#include "hpm_mchtmr_drv.h"
#include "hpm_l1c_drv.h"
#include "hpm_soc.h"
#include "hpm_interrupt.h"
#include <string.h>

/*============================================================================
 * Board-specific configuration (from board.h)
 *============================================================================*/

/* Use existing board definitions or provide defaults */
#ifndef BOARD_LCD_SPI
#define BOARD_LCD_SPI               HPM_SPI7
#endif

#ifndef BOARD_LCD_SPI_CLK_NAME
#define BOARD_LCD_SPI_CLK_NAME      clock_spi7
#endif

/* DMA configuration */
#ifndef BOARD_LCD_DMA
#define BOARD_LCD_DMA               HPM_HDMA
#endif

#ifndef BOARD_LCD_DMAMUX
#define BOARD_LCD_DMAMUX            HPM_DMAMUX
#endif

#ifndef BOARD_LCD_DMA_CH
#define BOARD_LCD_DMA_CH            0
#endif

#ifndef BOARD_LCD_DMA_MUX_CH
#define BOARD_LCD_DMA_MUX_CH        DMAMUX_MUXCFG_HDMA_MUX0
#endif

#ifndef BOARD_LCD_DMA_SRC
#define BOARD_LCD_DMA_SRC           HPM_DMA_SRC_SPI7_TX
#endif

#ifndef BOARD_LCD_DMA_IRQ
#define BOARD_LCD_DMA_IRQ           IRQn_HDMA
#endif

/* GPIO configuration */
#ifndef BOARD_LCD_GPIO
#define BOARD_LCD_GPIO              HPM_GPIO0
#endif

/* Default offsets for 172x320 screens */
#ifndef BOARD_LCD_X_OFFSET
#define BOARD_LCD_X_OFFSET          34
#endif

#ifndef BOARD_LCD_Y_OFFSET
#define BOARD_LCD_Y_OFFSET          0
#endif

/*============================================================================
 * Private data
 *============================================================================*/

/* Frame buffers - cache aligned */
static uint8_t __attribute__((aligned(64), section(".noncacheable"))) 
    lvgl_fb0[HPM_LVGL_FB_SIZE];

#if HPM_LVGL_USE_DOUBLE_BUFFER
static uint8_t __attribute__((aligned(64), section(".noncacheable"))) 
    lvgl_fb1[HPM_LVGL_FB_SIZE];
#endif

/* LVGL context */
static struct {
    lv_display_t *disp;
    volatile bool dma_busy;
    volatile uint32_t tick_ms;
    
    /* FPS measurement */
    uint32_t frame_count;
    uint32_t last_fps_tick;
    uint32_t fps;
    
    /* Flush statistics */
    volatile uint32_t flush_count;
    volatile uint64_t flush_bytes;
    volatile uint32_t last_flush_tick;
    lv_area_t last_flush_area;
} lvgl_ctx;

/* Timer frequency */
static uint32_t mchtmr_freq_khz = 0;

/*============================================================================
 * Tick management
 *============================================================================*/

static uint32_t lvgl_tick_get_cb(void)
{
    if (mchtmr_freq_khz == 0) {
        mchtmr_freq_khz = clock_get_frequency(clock_mchtmr0) / 1000;
    }
    return (uint32_t)(mchtmr_get_count(HPM_MCHTMR) / mchtmr_freq_khz);
}

void hpm_lvgl_spi_tick_inc(uint32_t ms)
{
    lvgl_ctx.tick_ms += ms;
}

uint32_t hpm_lvgl_spi_tick_get(void)
{
    return lvgl_tick_get_cb();
}

/* Alias for lv_conf.h custom tick */
uint32_t custom_tick_get(void)
{
    return lvgl_tick_get_cb();
}

/*============================================================================
 * DMA completion callback
 *============================================================================*/

static void lvgl_dma_done_cb(void *user_data)
{
    (void)user_data;
    
    lvgl_ctx.dma_busy = false;
    
    /* Notify LVGL that flush is complete */
    if (lvgl_ctx.disp) {
        lv_display_flush_ready(lvgl_ctx.disp);
    }
    
    /* FPS counting */
    lvgl_ctx.frame_count++;
}

/*============================================================================
 * LVGL flush callback
 *============================================================================*/

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t x1 = area->x1;
    uint16_t y1 = area->y1;
    uint16_t x2 = area->x2;
    uint16_t y2 = area->y2;
    uint32_t w = x2 - x1 + 1;
    uint32_t h = y2 - y1 + 1;
    uint32_t byte_len = w * h * HPM_LVGL_PIXEL_SIZE;
    
    /* Flush statistics */
    lvgl_ctx.flush_count++;
    lvgl_ctx.flush_bytes += byte_len;
    lvgl_ctx.last_flush_tick = lvgl_tick_get_cb();
    lv_area_copy(&lvgl_ctx.last_flush_area, area);
    
    /* Set display window */
    st7789_set_window(x1, y1, x2, y2);
    
    /* Start DMA transfer */
    lvgl_ctx.dma_busy = true;
    
    if (st7789_write_pixels_dma(px_map, byte_len, lvgl_dma_done_cb, NULL) != status_success) {
        /* DMA failed, fall back to blocking transfer */
        lvgl_ctx.dma_busy = false;
        st7789_write_pixels((const uint16_t *)px_map, w * h);
        lv_display_flush_ready(disp);
    }
}

/*============================================================================
 * Display initialization
 *============================================================================*/

static hpm_stat_t lvgl_display_hw_init(void)
{
    st7789_config_t lcd_cfg = {0};
    
    /* SPI configuration */
    lcd_cfg.spi_base = BOARD_LCD_SPI;
    lcd_cfg.spi_clk_name = BOARD_LCD_SPI_CLK_NAME;
    lcd_cfg.spi_freq_hz = HPM_LVGL_SPI_FREQ;
    
    /* DMA configuration */
    lcd_cfg.dma_base = BOARD_LCD_DMA;
    lcd_cfg.dmamux_base = BOARD_LCD_DMAMUX;
    lcd_cfg.dma_channel = BOARD_LCD_DMA_CH;
    lcd_cfg.dma_mux_channel = BOARD_LCD_DMA_MUX_CH;
    lcd_cfg.dma_src_request = BOARD_LCD_DMA_SRC;
    lcd_cfg.dma_irq_num = BOARD_LCD_DMA_IRQ;
    
    /* GPIO configuration */
    lcd_cfg.gpio_base = BOARD_LCD_GPIO;
    lcd_cfg.dc_gpio_index = BOARD_LCD_D_C_INDEX;
    lcd_cfg.dc_gpio_pin = BOARD_LCD_D_C_PIN;
    lcd_cfg.rst_gpio_index = BOARD_LCD_RESET_INDEX;
    lcd_cfg.rst_gpio_pin = BOARD_LCD_RESET_PIN;
    lcd_cfg.bl_gpio_index = BOARD_LCD_BL_INDEX;
    lcd_cfg.bl_gpio_pin = BOARD_LCD_BL_PIN;
    
    /* Display configuration */
    lcd_cfg.width = HPM_LVGL_LCD_WIDTH;
    lcd_cfg.height = HPM_LVGL_LCD_HEIGHT;
    lcd_cfg.x_offset = BOARD_LCD_X_OFFSET;
    lcd_cfg.y_offset = BOARD_LCD_Y_OFFSET;
    lcd_cfg.driver_ic = LCD_DRIVER_ST7789;  /* Also works for GC9307 */
    lcd_cfg.rotation = 0;
    lcd_cfg.invert_colors = true;           /* Most ST7789 displays need inversion */
    
    return st7789_init(&lcd_cfg);
}

/*============================================================================
 * DMA IRQ handler
 *============================================================================*/

void hpm_lvgl_spi_dma_irq_handler(void)
{
    st7789_dma_irq_handler();
}

/* Register DMA IRQ - this needs to be called or ISR defined in application */
SDK_DECLARE_EXT_ISR_M(BOARD_LCD_DMA_IRQ, hpm_lvgl_spi_dma_isr)
void hpm_lvgl_spi_dma_isr(void)
{
    hpm_lvgl_spi_dma_irq_handler();
}

/*============================================================================
 * Public API
 *============================================================================*/

lv_display_t *hpm_lvgl_spi_init(void)
{
    lv_display_t *disp;
    
    /* Clear context */
    memset(&lvgl_ctx, 0, sizeof(lvgl_ctx));
    
    /* Initialize LVGL */
    lv_init();
    
    /* Set tick callback */
    lv_tick_set_cb(lvgl_tick_get_cb);
    
    /* Initialize display hardware */
    if (lvgl_display_hw_init() != status_success) {
        return NULL;
    }
    
    /* Enable DMA interrupt */
    intc_m_enable_irq_with_priority(BOARD_LCD_DMA_IRQ, 5);
    
    /* Create LVGL display */
    disp = lv_display_create(HPM_LVGL_LCD_WIDTH, HPM_LVGL_LCD_HEIGHT);
    if (disp == NULL) {
        return NULL;
    }
    
    /* Configure buffers */
#if HPM_LVGL_USE_DOUBLE_BUFFER
    lv_display_set_buffers(disp, lvgl_fb0, lvgl_fb1, HPM_LVGL_FB_SIZE, 
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
#else
    lv_display_set_buffers(disp, lvgl_fb0, NULL, HPM_LVGL_FB_SIZE, 
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
    
    /* Set flush callback */
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    
    /* Store display reference */
    lvgl_ctx.disp = disp;
    lvgl_ctx.last_fps_tick = lvgl_tick_get_cb();
    
    return disp;
}

lv_display_t *hpm_lvgl_spi_get_display(void)
{
    return lvgl_ctx.disp;
}

void hpm_lvgl_spi_backlight(bool on)
{
    st7789_backlight(on);
}

void hpm_lvgl_spi_set_rotation(uint8_t rotation)
{
    st7789_set_rotation(rotation);
    
    /* Update LVGL display size if rotated 90/270 */
    if (lvgl_ctx.disp) {
        if (rotation == 90 || rotation == 270) {
            lv_display_set_resolution(lvgl_ctx.disp, HPM_LVGL_LCD_HEIGHT, HPM_LVGL_LCD_WIDTH);
        } else {
            lv_display_set_resolution(lvgl_ctx.disp, HPM_LVGL_LCD_WIDTH, HPM_LVGL_LCD_HEIGHT);
        }
    }
}

uint32_t hpm_lvgl_spi_get_fps(void)
{
    uint32_t now = lvgl_tick_get_cb();
    uint32_t elapsed = now - lvgl_ctx.last_fps_tick;
    
    if (elapsed >= 1000) {
        lvgl_ctx.fps = (lvgl_ctx.frame_count * 1000) / elapsed;
        lvgl_ctx.frame_count = 0;
        lvgl_ctx.last_fps_tick = now;
    }
    
    return lvgl_ctx.fps;
}

void hpm_lvgl_spi_reset_stats(void)
{
    lvgl_ctx.flush_count = 0;
    lvgl_ctx.flush_bytes = 0;
    lvgl_ctx.last_flush_tick = lvgl_tick_get_cb();
    memset(&lvgl_ctx.last_flush_area, 0, sizeof(lvgl_ctx.last_flush_area));
}

void hpm_lvgl_spi_get_stats(hpm_lvgl_spi_stats_t *out)
{
    if (out == NULL) {
        return;
    }

    out->flush_count = lvgl_ctx.flush_count;
    out->flush_bytes = lvgl_ctx.flush_bytes;
    out->last_flush_tick = lvgl_ctx.last_flush_tick;
    lv_area_copy(&out->last_flush_area, &lvgl_ctx.last_flush_area);
}
