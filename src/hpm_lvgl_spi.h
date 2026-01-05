/*
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * LVGL SPI Display Adapter for HPM6E00
 * Supports ST7789/GC9307 with DMA for 60FPS performance
 */

#ifndef HPM_LVGL_SPI_H
#define HPM_LVGL_SPI_H

#include "lvgl.h"
#include "st7789.h"

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
#define HPM_LVGL_SPI_FREQ       (40000000UL)    /* 40MHz for stability */
#endif

/* Buffer configuration */
#ifndef HPM_LVGL_USE_DOUBLE_BUFFER
#define HPM_LVGL_USE_DOUBLE_BUFFER  1           /* Enable double buffering */
#endif

/* Partial refresh - key for 60FPS */
#ifndef HPM_LVGL_USE_PARTIAL_REFRESH
#define HPM_LVGL_USE_PARTIAL_REFRESH 1
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
 * @note Call at least every 1-5ms for smooth animations
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
 * @param rotation 0, 90, 180, or 270
 */
void hpm_lvgl_spi_set_rotation(uint8_t rotation);

/**
 * @brief Get actual FPS (for debugging)
 * @return uint32_t Current FPS
 */
uint32_t hpm_lvgl_spi_get_fps(void);

/**
 * @brief DMA IRQ handler - must be called from DMA ISR
 */
void hpm_lvgl_spi_dma_irq_handler(void);

#endif /* HPM_LVGL_SPI_H */
