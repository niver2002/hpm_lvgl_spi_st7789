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
#include <stddef.h>
#include <string.h>

/* LVGL built-in ST7789 (generic MIPI) driver lives under:
 * middleware/lvgl/lvgl/src/drivers/display/st7789 */
#if HPM_LVGL_USE_LVGL_ST7789_DRIVER
#include "src/drivers/display/st7789/lv_st7789.h"
#endif

#if !HPM_LVGL_USE_LVGL_ST7789_DRIVER
#include "st7789.h"
#endif

/* When using LVGL's built-in ST7789 driver, this component currently expects the HPM SDK SPI component
 * (`components/spi/hpm_spi`) + DMA manager (`components/dma_mgr`) to provide DMA-backed non-blocking transfers.
 *
 * Rationale: DMA manager owns `IRQn_HDMA`/`IRQn_XDMA` ISRs, so we must not declare a competing DMA ISR here. */
#if HPM_LVGL_USE_LVGL_ST7789_DRIVER
#if !USE_DMA_MGR
#error "HPM_LVGL_USE_LVGL_ST7789_DRIVER requires USE_DMA_MGR=1 (enable HPM SDK CONFIG_DMA_MGR + components/spi)."
#endif
#if !defined(LV_USE_GENERIC_MIPI) || (LV_USE_GENERIC_MIPI == 0)
#error "LV_USE_GENERIC_MIPI must be enabled in LVGL config when using the LVGL ST7789 (generic MIPI) driver."
#endif
#if !defined(LV_USE_ST7789) || (LV_USE_ST7789 == 0)
#error "LV_USE_ST7789 must be enabled in LVGL config when using the LVGL ST7789 driver."
#endif
#endif

/* Protect users from enabling DMA manager while forcing legacy driver: the DMA IRQ would conflict. */
#if USE_DMA_MGR && !HPM_LVGL_USE_LVGL_ST7789_DRIVER
#error "USE_DMA_MGR=1 conflicts with legacy DMAv2 ISR path. Set HPM_LVGL_USE_LVGL_ST7789_DRIVER=1 or disable DMA manager."
#endif

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

/* Optional GPIO CS (recommended when sharing SPI bus).
 * If not defined, CS is assumed to be handled externally (e.g. tied low or SPI HW CS). */
#if defined(BOARD_LCD_CS_INDEX) && defined(BOARD_LCD_CS_PIN)
#ifndef BOARD_LCD_CS_ACTIVE_LEVEL
#define BOARD_LCD_CS_ACTIVE_LEVEL   0 /* active low */
#endif
#define HPM_LVGL_SPI_HAS_GPIO_CS    1
#else
#define HPM_LVGL_SPI_HAS_GPIO_CS    0
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
static uint8_t HPM_LVGL_FB_ATTR lvgl_fb0[HPM_LVGL_FB_SIZE];

#if HPM_LVGL_USE_DOUBLE_BUFFER
static uint8_t HPM_LVGL_FB_ATTR lvgl_fb1[HPM_LVGL_FB_SIZE];
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
 * LCD GPIO helpers (D/C, CS, RST, BL)
 *============================================================================*/

static inline void lcd_dc_command(void)
{
    gpio_write_pin(BOARD_LCD_GPIO, BOARD_LCD_D_C_INDEX, BOARD_LCD_D_C_PIN, 0);
}

static inline void lcd_dc_data(void)
{
    gpio_write_pin(BOARD_LCD_GPIO, BOARD_LCD_D_C_INDEX, BOARD_LCD_D_C_PIN, 1);
}

static inline void lcd_cs_assert(void)
{
#if HPM_LVGL_SPI_HAS_GPIO_CS
    gpio_write_pin(BOARD_LCD_GPIO, BOARD_LCD_CS_INDEX, BOARD_LCD_CS_PIN, BOARD_LCD_CS_ACTIVE_LEVEL);
#endif
}

static inline void lcd_cs_deassert(void)
{
#if HPM_LVGL_SPI_HAS_GPIO_CS
    gpio_write_pin(BOARD_LCD_GPIO, BOARD_LCD_CS_INDEX, BOARD_LCD_CS_PIN, !BOARD_LCD_CS_ACTIVE_LEVEL);
#endif
}

static inline void lcd_backlight_set(bool on)
{
#if defined(BOARD_LCD_BL_INDEX) && defined(BOARD_LCD_BL_PIN)
    gpio_write_pin(BOARD_LCD_GPIO, BOARD_LCD_BL_INDEX, BOARD_LCD_BL_PIN, on ? 1 : 0);
#else
    (void)on;
#endif
}

static void lcd_hw_reset(void)
{
#if defined(BOARD_LCD_RESET_INDEX) && defined(BOARD_LCD_RESET_PIN)
    gpio_write_pin(BOARD_LCD_GPIO, BOARD_LCD_RESET_INDEX, BOARD_LCD_RESET_PIN, 1);
    board_delay_ms(10);
    gpio_write_pin(BOARD_LCD_GPIO, BOARD_LCD_RESET_INDEX, BOARD_LCD_RESET_PIN, 0);
    board_delay_ms(10);
    gpio_write_pin(BOARD_LCD_GPIO, BOARD_LCD_RESET_INDEX, BOARD_LCD_RESET_PIN, 1);
    board_delay_ms(120);
#endif
}

static inline void lcd_spi_wait_transfer_done(SPI_Type *spi)
{
    while (spi_get_tx_fifo_valid_data_size(spi) != 0U) {
    }
    while (spi_is_active(spi)) {
    }
}

/*============================================================================
 * Tick management
 *============================================================================*/

static uint32_t lvgl_tick_get_cb(void)
{
#if HPM_LVGL_TICK_SOURCE_MCHTMR
    if (mchtmr_freq_khz == 0) {
        mchtmr_freq_khz = clock_get_frequency(clock_mchtmr0) / 1000;
    }
    return (uint32_t)(mchtmr_get_count(HPM_MCHTMR) / mchtmr_freq_khz);
#else
    return lvgl_ctx.tick_ms;
#endif
}

void hpm_lvgl_spi_tick_inc(uint32_t ms)
{
#if HPM_LVGL_TICK_SOURCE_MCHTMR
    /* Using hardware tick source; nothing to do. */
    (void)ms;
#else
    lvgl_ctx.tick_ms += ms;
#endif
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

#if HPM_LVGL_USE_LVGL_ST7789_DRIVER
#include "hpm_spi.h"
#include "hpm_dma_mgr.h"

/* Track last address window (parsed from MIPI DCS CASET/RASET) to provide meaningful stats. */
static struct {
    uint16_t x1_vram;
    uint16_t x2_vram; /* inclusive, as sent to panel */
    uint16_t y1_vram;
    uint16_t y2_vram; /* inclusive, as sent to panel */
    bool has_x;
    bool has_y;
} lcd_addr_state;

typedef struct {
    SPI_Type *spi;
    lv_display_t *disp;
} hpm_lvgl_spi_dma_done_ctx_t;

static hpm_lvgl_spi_dma_done_ctx_t lvgl_dma_done_ctx;

static void hpm_lvgl_spi_dma_tc_cb(DMA_Type *base, uint32_t channel, void *cb_data_ptr)
{
    (void)base;
    (void)channel;

    hpm_lvgl_spi_dma_done_ctx_t *ctx = (hpm_lvgl_spi_dma_done_ctx_t *)cb_data_ptr;
    if ((ctx == NULL) || (ctx->spi == NULL)) {
        return;
    }

    /* DMA TC only means FIFO writes are done; wait for SPI shifter to finish. */
    lcd_spi_wait_transfer_done(ctx->spi);

    /* Release chip select after actual bus idle. */
    lcd_cs_deassert();

    lvgl_ctx.dma_busy = false;

    /* Notify LVGL that flush is complete */
    if (ctx->disp) {
        lv_display_flush_ready(ctx->disp);
    }

    /* FPS counting */
    lvgl_ctx.frame_count++;
}

static inline void lvgl_update_last_flush_area_from_mipi_state(void)
{
    if ((!lcd_addr_state.has_x) || (!lcd_addr_state.has_y)) {
        return;
    }

    /* Map VRAM coordinates back to LVGL coordinates by subtracting configured gap. */
    lvgl_ctx.last_flush_area.x1 = (int32_t)lcd_addr_state.x1_vram - (int32_t)BOARD_LCD_X_OFFSET;
    lvgl_ctx.last_flush_area.x2 = (int32_t)lcd_addr_state.x2_vram - (int32_t)BOARD_LCD_X_OFFSET;
    lvgl_ctx.last_flush_area.y1 = (int32_t)lcd_addr_state.y1_vram - (int32_t)BOARD_LCD_Y_OFFSET;
    lvgl_ctx.last_flush_area.y2 = (int32_t)lcd_addr_state.y2_vram - (int32_t)BOARD_LCD_Y_OFFSET;
}

static void lvgl_lcd_send_cmd_cb(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size,
                                const uint8_t *param, size_t param_size)
{
    (void)disp;

    if ((cmd == NULL) || (cmd_size == 0U)) {
        return;
    }

    /* Capture last address window for stats (generic MIPI flush sends CASET then RASET). */
    if ((cmd_size == 1U) && (param != NULL) && (param_size == 4U)) {
        if (cmd[0] == LV_LCD_CMD_SET_COLUMN_ADDRESS) {
            lcd_addr_state.x1_vram = ((uint16_t)param[0] << 8) | param[1];
            lcd_addr_state.x2_vram = ((uint16_t)param[2] << 8) | param[3];
            lcd_addr_state.has_x = true;
        } else if (cmd[0] == LV_LCD_CMD_SET_PAGE_ADDRESS) {
            lcd_addr_state.y1_vram = ((uint16_t)param[0] << 8) | param[1];
            lcd_addr_state.y2_vram = ((uint16_t)param[2] << 8) | param[3];
            lcd_addr_state.has_y = true;
        }
    }

    lcd_cs_assert();

    lcd_dc_command();
    if (hpm_spi_transmit_blocking(BOARD_LCD_SPI, (uint8_t *)cmd, cmd_size, 1000) != status_success) {
        lcd_cs_deassert();
        return;
    }

    if ((param != NULL) && (param_size != 0U)) {
        lcd_dc_data();
        (void)hpm_spi_transmit_blocking(BOARD_LCD_SPI, (uint8_t *)param, param_size, 1000);
    }

    lcd_spi_wait_transfer_done(BOARD_LCD_SPI);
    lcd_cs_deassert();
}

static void lvgl_lcd_send_color_cb(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size, uint8_t *param,
                                  size_t param_size)
{
    if ((disp == NULL) || (cmd == NULL) || (cmd_size == 0U) || (param == NULL) || (param_size == 0U)) {
        if (disp) {
            lv_display_flush_ready(disp);
        }
        return;
    }

    /* Flush statistics (derive area from last CASET/RASET). */
    lvgl_ctx.flush_count++;
    lvgl_ctx.flush_bytes += param_size;
    lvgl_ctx.last_flush_tick = lvgl_tick_get_cb();
    lvgl_update_last_flush_area_from_mipi_state();

    /* Record the display for DMA completion callback. */
    lvgl_dma_done_ctx.disp = disp;

    lcd_cs_assert();

    /* Send the RAMWR command first (polling) */
    lcd_dc_command();
    if (hpm_spi_transmit_blocking(BOARD_LCD_SPI, (uint8_t *)cmd, cmd_size, 1000) != status_success) {
        lcd_cs_deassert();
        lv_display_flush_ready(disp);
        return;
    }

    lcd_spi_wait_transfer_done(BOARD_LCD_SPI);

    /* Ensure data buffer is visible to DMA when using cacheable memory. */
    if (l1c_dc_is_enabled()) {
        uint32_t aligned_start = HPM_L1C_CACHELINE_ALIGN_DOWN((uint32_t)param);
        uint32_t aligned_end = HPM_L1C_CACHELINE_ALIGN_UP((uint32_t)param + (uint32_t)param_size);
        uint32_t aligned_size = aligned_end - aligned_start;
        l1c_dc_writeback(aligned_start, aligned_size);
    }

    /* Start pixel transfer using DMA (non-blocking). CS remains asserted until DMA callback. */
    lcd_dc_data();
    lvgl_ctx.dma_busy = true;
    if (hpm_spi_transmit_nonblocking(BOARD_LCD_SPI, param, param_size) != status_success) {
        /* DMA failed, fall back to blocking transfer (always release CS + flush_ready). */
        lvgl_ctx.dma_busy = false;
        (void)hpm_spi_transmit_blocking(BOARD_LCD_SPI, param, param_size, 1000);
        lcd_spi_wait_transfer_done(BOARD_LCD_SPI);
        lcd_cs_deassert();
        lv_display_flush_ready(disp);
        lvgl_ctx.frame_count++;
    }
}

static hpm_stat_t lvgl_display_hw_init(void)
{
    spi_initialize_config_t spi_cfg;

    /* Initialize LCD control GPIOs (pinmux must be done by board_init_lcd()). */
    gpio_set_pin_output(BOARD_LCD_GPIO, BOARD_LCD_D_C_INDEX, BOARD_LCD_D_C_PIN);
#if defined(BOARD_LCD_RESET_INDEX) && defined(BOARD_LCD_RESET_PIN)
    gpio_set_pin_output(BOARD_LCD_GPIO, BOARD_LCD_RESET_INDEX, BOARD_LCD_RESET_PIN);
#endif
#if defined(BOARD_LCD_BL_INDEX) && defined(BOARD_LCD_BL_PIN)
    gpio_set_pin_output(BOARD_LCD_GPIO, BOARD_LCD_BL_INDEX, BOARD_LCD_BL_PIN);
    lcd_backlight_set(false);
#endif
#if HPM_LVGL_SPI_HAS_GPIO_CS
    gpio_set_pin_output(BOARD_LCD_GPIO, BOARD_LCD_CS_INDEX, BOARD_LCD_CS_PIN);
    lcd_cs_deassert();
#endif

    lcd_hw_reset();

    /* Initialize DMA manager + SPI component backend. */
    dma_mgr_init();

    /* Enable SPI clock (required for hpm_spi_set_sclk_frequency). */
    clock_add_to_group(BOARD_LCD_SPI_CLK_NAME, 0);

    hpm_spi_get_default_init_config(&spi_cfg);
    if (hpm_spi_initialize(BOARD_LCD_SPI, &spi_cfg) != status_success) {
        return status_fail;
    }
    if (hpm_spi_set_sclk_frequency(BOARD_LCD_SPI, HPM_LVGL_SPI_FREQ) != status_success) {
        return status_fail;
    }

    /* Register DMA completion callback for TX channel. */
    lvgl_dma_done_ctx.spi = BOARD_LCD_SPI;
    lvgl_dma_done_ctx.disp = NULL;
    if (hpm_spi_tx_dma_mgr_install_custom_callback(BOARD_LCD_SPI, hpm_lvgl_spi_dma_tc_cb, &lvgl_dma_done_ctx) != status_success) {
        return status_fail;
    }

    return status_success;
}
#else /* !HPM_LVGL_USE_LVGL_ST7789_DRIVER */

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
 * LVGL flush callback (legacy st7789.c driver)
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
#endif /* HPM_LVGL_USE_LVGL_ST7789_DRIVER */

/*============================================================================
 * DMA IRQ handler
 *============================================================================*/

void hpm_lvgl_spi_dma_irq_handler(void)
{
#if !HPM_LVGL_USE_LVGL_ST7789_DRIVER
    st7789_dma_irq_handler();
#endif
}

#if !USE_DMA_MGR && !HPM_LVGL_USE_LVGL_ST7789_DRIVER
/* Register DMA IRQ for legacy DMAv2 path.
 * NOTE: Not used when DMA manager is enabled (dma_mgr owns IRQn_HDMA/IRQn_XDMA ISRs). */
SDK_DECLARE_EXT_ISR_M(BOARD_LCD_DMA_IRQ, hpm_lvgl_spi_dma_isr)
void hpm_lvgl_spi_dma_isr(void)
{
    hpm_lvgl_spi_dma_irq_handler();
}
#endif

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

#if !HPM_LVGL_USE_LVGL_ST7789_DRIVER
    /* Enable DMA interrupt (legacy DMAv2 path). */
    intc_m_enable_irq_with_priority(BOARD_LCD_DMA_IRQ, 5);

    /* Create LVGL display */
    disp = lv_display_create(HPM_LVGL_LCD_WIDTH, HPM_LVGL_LCD_HEIGHT);
    if (disp == NULL) {
        return NULL;
    }
#else
    /* Create LVGL display (LVGL built-in ST7789 wrapper uses generic MIPI driver). */
    disp = lv_st7789_create(HPM_LVGL_LCD_WIDTH, HPM_LVGL_LCD_HEIGHT, (lv_lcd_flag_t)HPM_LVGL_LCD_FLAGS,
                            lvgl_lcd_send_cmd_cb, lvgl_lcd_send_color_cb);
    if (disp == NULL) {
        return NULL;
    }

    lv_st7789_set_gap(disp, BOARD_LCD_X_OFFSET, BOARD_LCD_Y_OFFSET);
    lv_st7789_set_invert(disp, HPM_LVGL_LCD_INVERT);
#endif
    
    /* Configure buffers */
#if HPM_LVGL_USE_DOUBLE_BUFFER
    lv_display_set_buffers(disp, lvgl_fb0, lvgl_fb1, HPM_LVGL_FB_SIZE, 
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
#else
    lv_display_set_buffers(disp, lvgl_fb0, NULL, HPM_LVGL_FB_SIZE, 
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
    
    /* Set flush callback */
#if !HPM_LVGL_USE_LVGL_ST7789_DRIVER
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
#endif
    
    /* Store display reference */
    lvgl_ctx.disp = disp;
    lvgl_ctx.last_fps_tick = lvgl_tick_get_cb();

#if HPM_LVGL_USE_LVGL_ST7789_DRIVER
    /* Turn on backlight after successful init */
    lcd_backlight_set(true);
#endif
    
    return disp;
}

lv_display_t *hpm_lvgl_spi_get_display(void)
{
    return lvgl_ctx.disp;
}

void hpm_lvgl_spi_backlight(bool on)
{
#if HPM_LVGL_USE_LVGL_ST7789_DRIVER
    lcd_backlight_set(on);
#else
    st7789_backlight(on);
#endif
}

void hpm_lvgl_spi_set_rotation(uint8_t rotation)
{
#if HPM_LVGL_USE_LVGL_ST7789_DRIVER
    if (!lvgl_ctx.disp) {
        return;
    }

    switch (rotation) {
    case 0:
        lv_display_set_rotation(lvgl_ctx.disp, LV_DISPLAY_ROTATION_0);
        break;
    case 90:
        lv_display_set_rotation(lvgl_ctx.disp, LV_DISPLAY_ROTATION_90);
        break;
    case 180:
        lv_display_set_rotation(lvgl_ctx.disp, LV_DISPLAY_ROTATION_180);
        break;
    case 270:
        lv_display_set_rotation(lvgl_ctx.disp, LV_DISPLAY_ROTATION_270);
        break;
    default:
        break;
    }
#else
    st7789_set_rotation(rotation);

    /* Update LVGL display size if rotated 90/270 */
    if (lvgl_ctx.disp) {
        if (rotation == 90 || rotation == 270) {
            lv_display_set_resolution(lvgl_ctx.disp, HPM_LVGL_LCD_HEIGHT, HPM_LVGL_LCD_WIDTH);
        } else {
            lv_display_set_resolution(lvgl_ctx.disp, HPM_LVGL_LCD_WIDTH, HPM_LVGL_LCD_HEIGHT);
        }
    }
#endif
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
