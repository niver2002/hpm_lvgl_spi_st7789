/*
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ST7789/GC9307 SPI LCD Driver for HPM6E00
 * Optimized for 60FPS with DMA
 */

#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include <stdbool.h>
#include "hpm_common.h"
#include "hpm_spi_drv.h"
#include "hpm_dmav2_drv.h"
#include "hpm_dmamux_drv.h"
#include "hpm_gpio_drv.h"

/*============================================================================
 * Configuration - Adjust for your hardware
 *============================================================================*/

/* Screen resolution */
#ifndef ST7789_WIDTH
#define ST7789_WIDTH        172
#endif

#ifndef ST7789_HEIGHT
#define ST7789_HEIGHT       320
#endif

/* Color format */
#define ST7789_COLOR_RGB565     0x55
#define ST7789_COLOR_RGB666     0x66

#ifndef ST7789_COLOR_MODE
#define ST7789_COLOR_MODE   ST7789_COLOR_RGB565
#endif

/* Display offset (some screens need this) */
#ifndef ST7789_X_OFFSET
#define ST7789_X_OFFSET     34      /* Common for 172x320 screens */
#endif

#ifndef ST7789_Y_OFFSET
#define ST7789_Y_OFFSET     0
#endif

/* Driver IC selection */
typedef enum {
    LCD_DRIVER_ST7789 = 0,
    LCD_DRIVER_GC9307,
} lcd_driver_ic_t;

/*============================================================================
 * ST7789 Commands
 *============================================================================*/
#define ST7789_NOP          0x00
#define ST7789_SWRESET      0x01
#define ST7789_RDDID        0x04
#define ST7789_RDDST        0x09

#define ST7789_SLPIN        0x10
#define ST7789_SLPOUT       0x11
#define ST7789_PTLON        0x12
#define ST7789_NORON        0x13

#define ST7789_INVOFF       0x20
#define ST7789_INVON        0x21
#define ST7789_DISPOFF      0x28
#define ST7789_DISPON       0x29
#define ST7789_CASET        0x2A
#define ST7789_RASET        0x2B
#define ST7789_RAMWR        0x2C
#define ST7789_RAMRD        0x2E

#define ST7789_PTLAR        0x30
#define ST7789_VSCRDEF      0x33
#define ST7789_TEOFF        0x34
#define ST7789_TEON         0x35
#define ST7789_MADCTL       0x36
#define ST7789_VSCSAD       0x37
#define ST7789_IDMOFF       0x38
#define ST7789_IDMON        0x39
#define ST7789_COLMOD       0x3A

#define ST7789_RAMCTRL      0xB0
#define ST7789_RGBCTRL      0xB1
#define ST7789_PORCTRL      0xB2
#define ST7789_FRCTRL1      0xB3
#define ST7789_PARCTRL      0xB5
#define ST7789_GCTRL        0xB7
#define ST7789_GTADJ        0xB8
#define ST7789_DGMEN        0xBA
#define ST7789_VCOMS        0xBB
#define ST7789_LCMCTRL      0xC0
#define ST7789_IDSET        0xC1
#define ST7789_VDVVRHEN     0xC2
#define ST7789_VRHS         0xC3
#define ST7789_VDVS         0xC4
#define ST7789_VCMOFSET     0xC5
#define ST7789_FRCTRL2      0xC6
#define ST7789_CABCCTRL     0xC7
#define ST7789_REGSEL1      0xC8
#define ST7789_REGSEL2      0xCA
#define ST7789_PWMFRSEL     0xCC
#define ST7789_PWCTRL1      0xD0
#define ST7789_VAPVANEN     0xD2
#define ST7789_CMD2EN       0xDF

#define ST7789_PVGAMCTRL    0xE0
#define ST7789_NVGAMCTRL    0xE1
#define ST7789_DGMLUTR      0xE2
#define ST7789_DGMLUTB      0xE3
#define ST7789_GATECTRL     0xE4
#define ST7789_SPI2EN       0xE7
#define ST7789_PWCTRL2      0xE8
#define ST7789_EQCTRL       0xE9
#define ST7789_PROMCTRL     0xEC
#define ST7789_PROMEN       0xFA
#define ST7789_NVMSET       0xFC
#define ST7789_PROMACT      0xFE

/* MADCTL bits */
#define ST7789_MADCTL_MY    0x80    /* Row address order */
#define ST7789_MADCTL_MX    0x40    /* Column address order */
#define ST7789_MADCTL_MV    0x20    /* Row/Column exchange */
#define ST7789_MADCTL_ML    0x10    /* Vertical refresh order */
#define ST7789_MADCTL_BGR   0x08    /* BGR order */
#define ST7789_MADCTL_MH    0x04    /* Horizontal refresh order */
#define ST7789_MADCTL_RGB   0x00    /* RGB order */

/*============================================================================
 * Hardware configuration structure
 *============================================================================*/
typedef struct {
    /* SPI configuration */
    SPI_Type *spi_base;
    uint32_t spi_clk_name;
    uint32_t spi_freq_hz;           /* Target SPI frequency (check panel + signal integrity) */
    
    /* DMA configuration */
    DMA_Type *dma_base;
    DMAMUX_Type *dmamux_base;
    uint8_t dma_channel;
    uint8_t dma_mux_channel;
    uint8_t dma_src_request;        /* SPI TX DMA request */
    uint32_t dma_irq_num;
    
    /* GPIO pins */
    GPIO_Type *gpio_base;
    uint32_t dc_gpio_index;         /* D/C pin GPIO port index */
    uint32_t dc_gpio_pin;           /* D/C pin number */
    uint32_t rst_gpio_index;        /* RST pin GPIO port index */
    uint32_t rst_gpio_pin;          /* RST pin number */
    uint32_t bl_gpio_index;         /* Backlight GPIO port index */
    uint32_t bl_gpio_pin;           /* Backlight pin number */
    
    /* Display configuration */
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    lcd_driver_ic_t driver_ic;
    uint8_t rotation;               /* 0, 90, 180, 270 */
    bool invert_colors;
} st7789_config_t;

/* DMA transfer completion callback */
typedef void (*st7789_dma_done_cb_t)(void *user_data);

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize ST7789/GC9307 display
 * @param config Hardware configuration
 * @return status_success on success
 */
hpm_stat_t st7789_init(const st7789_config_t *config);

/**
 * @brief Set display window for pixel writes
 * @param x0, y0 Top-left corner
 * @param x1, y1 Bottom-right corner
 */
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Fill area with solid color (blocking)
 * @param x0, y0, x1, y1 Area coordinates
 * @param color RGB565 color
 */
void st7789_fill_area(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

/**
 * @brief Write pixel data (blocking, no DMA)
 * @param data Pointer to RGB565 pixel data
 * @param len Length in bytes
 */
void st7789_write_pixels(const uint16_t *data, uint32_t pixel_count);

/**
 * @brief Write pixel data via DMA (non-blocking)
 * @param data Pointer to RGB565 pixel data (must be cache-aligned)
 * @param len Length in bytes
 * @param callback Function to call when DMA completes
 * @param user_data User data passed to callback
 * @note DMA terminal-count does not necessarily mean the SPI bus has finished shifting out
 *       the last bits. This driver waits for SPI to become idle (spi_is_active == false)
 *       before invoking the callback.
 * @return status_success if DMA transfer started
 */
hpm_stat_t st7789_write_pixels_dma(const void *data, uint32_t byte_len, 
                                    st7789_dma_done_cb_t callback, void *user_data);

/**
 * @brief Check if DMA transfer is in progress
 * @return true if busy
 */
bool st7789_is_busy(void);

/**
 * @brief Wait for DMA transfer to complete
 */
void st7789_wait_idle(void);

/**
 * @brief Set display rotation
 * @param rotation 0, 90, 180, or 270 degrees
 */
void st7789_set_rotation(uint16_t rotation);

/**
 * @brief Turn display on/off
 * @param on true to turn on
 */
void st7789_display_on(bool on);

/**
 * @brief Set backlight
 * @param on true to turn on backlight
 */
void st7789_backlight(bool on);

/**
 * @brief Invert display colors
 * @param invert true to invert
 */
void st7789_invert(bool invert);

/**
 * @brief Get display width (accounting for rotation)
 */
uint16_t st7789_get_width(void);

/**
 * @brief Get display height (accounting for rotation)
 */
uint16_t st7789_get_height(void);

/**
 * @brief DMA IRQ handler - call from your DMA ISR
 */
void st7789_dma_irq_handler(void);

#endif /* ST7789_H */
