/*
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * LVGL SPI Display Adapter for HPM6E00
 * Supports ST7789/GC9307 with DMA for 60FPS performance
 */

#ifndef HPM_LVGL_SPI_H
#define HPM_LVGL_SPI_H

#include "hpm_common.h"
#include "lvgl.h"

/* If using the HPM SDK `components/spi` driver with DMA manager, the SDK will define `USE_DMA_MGR=1`.
 * Provide a safe default for projects that don't enable DMA manager. */
#ifndef USE_DMA_MGR
#define USE_DMA_MGR 0
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

/* Screen dimensions */
#ifndef HPM_LVGL_LCD_WIDTH
#define HPM_LVGL_LCD_WIDTH      172
#endif

#ifndef HPM_LVGL_LCD_HEIGHT
#define HPM_LVGL_LCD_HEIGHT     320
#endif

/* SPI configuration */
#ifndef HPM_LVGL_SPI_FREQ
#ifdef BOARD_LCD_SPI_CLK_FREQ
/* Prefer board default when available (e.g. hpm_apps boards define BOARD_LCD_SPI_CLK_FREQ). */
#define HPM_LVGL_SPI_FREQ       BOARD_LCD_SPI_CLK_FREQ
#else
#define HPM_LVGL_SPI_FREQ       (40000000UL)    /* 40MHz is usually stable with short wires */
#endif
#endif

/* Buffer configuration */
#ifndef HPM_LVGL_USE_DOUBLE_BUFFER
#define HPM_LVGL_USE_DOUBLE_BUFFER  1           /* Enable double buffering */
#endif

/* Tick source:
 * - 1: Use MCHTMR (hardware timer) as LVGL tick source (recommended on HPM6E).
 * - 0: Use a software counter; user must call hpm_lvgl_spi_tick_inc().
 */
#ifndef HPM_LVGL_TICK_SOURCE_MCHTMR
#define HPM_LVGL_TICK_SOURCE_MCHTMR 1
#endif

/* Partial refresh - key for 60FPS */
#ifndef HPM_LVGL_USE_PARTIAL_REFRESH
#define HPM_LVGL_USE_PARTIAL_REFRESH 1
#endif

/* Implementation selection:
 * - 1: Use LVGL's built-in ST7789 (generic MIPI) driver + platform callbacks (recommended; matches HPM SDK upstream).
 * - 0: Use legacy local `st7789.c` driver + custom flush callback.
 */
#ifndef HPM_LVGL_USE_LVGL_ST7789_DRIVER
#define HPM_LVGL_USE_LVGL_ST7789_DRIVER USE_DMA_MGR
#endif

/* LVGL ST7789 (generic MIPI) configuration (only used when `HPM_LVGL_USE_LVGL_ST7789_DRIVER == 1`).
 *
 * `HPM_LVGL_LCD_FLAGS` maps to `lv_lcd_flag_t` (e.g. `LV_LCD_FLAG_BGR`, `LV_LCD_FLAG_MIRROR_X`).
 * Default is `0` (no special flags).
 */
#ifndef HPM_LVGL_LCD_FLAGS
#define HPM_LVGL_LCD_FLAGS 0
#endif

/* Many ST7789 panels require color inversion (INVON). */
#ifndef HPM_LVGL_LCD_INVERT
#define HPM_LVGL_LCD_INVERT 1
#endif

/* Buffer size for partial refresh
 * 40MHz SPI performance:
 * - 80 lines (172x80x2=27.5KB): ~5.5ms transfer, good balance
 * - LVGL automatically handles dirty rectangle merging
 * - Larger buffer = fewer flush calls, better for scattered updates
 */
#define HPM_LVGL_PIXEL_SIZE     (LV_COLOR_DEPTH / 8)
#define HPM_LVGL_FB_LINES       80      /* 1/4 screen height */
#define HPM_LVGL_FB_SIZE        (HPM_LVGL_LCD_WIDTH * HPM_LVGL_FB_LINES * HPM_LVGL_PIXEL_SIZE)

/* DMA/LVGL draw buffers should be cache-safe. By default we place buffers into
 * a non-cacheable section and align to 64 bytes (HPM6E D-Cache line size).
 *
 * You can override this macro to match your linker script / toolchain.
 */
#ifndef HPM_LVGL_FB_ATTR
#if defined(ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT)
#define HPM_LVGL_FB_ATTR ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(64)
#else
#define HPM_LVGL_FB_ATTR __attribute__((aligned(64), section(".noncacheable")))
#endif
#endif

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize LVGL with SPI display
 * 
 * This function:
 * - Initializes LVGL core
 * - Configures ST7789/GC9307 display via SPI
 * - Sets up DMA for async transfers
 * - Configures double buffering
 * 
 * @return lv_display_t* Display object, or NULL on failure
 */
lv_display_t *hpm_lvgl_spi_init(void);

/**
 * @brief Get the LVGL display object
 * @return lv_display_t* Display object
 */
lv_display_t *hpm_lvgl_spi_get_display(void);

/**
 * @brief LVGL tick handler - call this from timer interrupt or main loop
 * @note Only required when `HPM_LVGL_TICK_SOURCE_MCHTMR == 0`.
 *       When using MCHTMR tick source, this function is a no-op.
 */
void hpm_lvgl_spi_tick_inc(uint32_t ms);

/**
 * @brief Get current tick count in milliseconds
 * @return uint32_t Tick count
 */
uint32_t hpm_lvgl_spi_tick_get(void);

/**
 * @brief Set display backlight
 * @param on true to turn on
 */
void hpm_lvgl_spi_backlight(bool on);

/**
 * @brief Set display rotation
 * @param rotation 0, 90, 180, or 270 (degrees)
 */
void hpm_lvgl_spi_set_rotation(uint16_t rotation);

/**
 * @brief Get actual FPS (for debugging)
 * @return uint32_t Current FPS
 */
uint32_t hpm_lvgl_spi_get_fps(void);

/**
 * @brief DMA IRQ handler - must be called from DMA ISR
 * @note Not required when `USE_DMA_MGR == 1` (DMA manager installs and handles IRQs).
 */
void hpm_lvgl_spi_dma_irq_handler(void);

/*============================================================================
 * Performance statistics (optional)
 *============================================================================*/

typedef struct {
    uint32_t flush_count;        /* Total flush calls since last reset */
    uint64_t flush_bytes;        /* Total bytes requested to flush */
    lv_area_t last_flush_area;   /* Last flushed area (LVGL coordinates) */
    uint32_t last_flush_tick;    /* Tick (ms) when last flush started */
} hpm_lvgl_spi_stats_t;

/**
 * @brief Reset flush statistics counters
 */
void hpm_lvgl_spi_reset_stats(void);

/**
 * @brief Get current flush statistics
 * @param out Output stats (must not be NULL)
 */
void hpm_lvgl_spi_get_stats(hpm_lvgl_spi_stats_t *out);

#endif /* HPM_LVGL_SPI_H */
