/*
 * Copyright (c) 2024 HPMicro
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ST7789/GC9307 SPI LCD Driver Implementation
 */

#include "st7789.h"
#include "hpm_clock_drv.h"
#include "hpm_l1c_drv.h"
#include "board.h"
#include <string.h>

#ifndef BOARD_RUNNING_CORE
#define BOARD_RUNNING_CORE HPM_CORE0
#endif

/*============================================================================
 * Private data
 *============================================================================*/
static struct {
    st7789_config_t cfg;
    volatile bool dma_busy;
    st7789_dma_done_cb_t dma_callback;
    void *dma_user_data;
    uint8_t rotation;
    uint16_t width;
    uint16_t height;
} st7789_ctx;

/*============================================================================
 * Low-level SPI operations
 *============================================================================*/
static inline void st7789_cs_low(void)
{
    /* CS is handled by SPI controller in most cases */
}

static inline void st7789_cs_high(void)
{
    /* CS is handled by SPI controller in most cases */
}

static inline void st7789_dc_command(void)
{
    gpio_write_pin(st7789_ctx.cfg.gpio_base, st7789_ctx.cfg.dc_gpio_index, 
                   st7789_ctx.cfg.dc_gpio_pin, 0);
}

static inline void st7789_dc_data(void)
{
    gpio_write_pin(st7789_ctx.cfg.gpio_base, st7789_ctx.cfg.dc_gpio_index, 
                   st7789_ctx.cfg.dc_gpio_pin, 1);
}

static inline void st7789_rst_low(void)
{
    gpio_write_pin(st7789_ctx.cfg.gpio_base, st7789_ctx.cfg.rst_gpio_index, 
                   st7789_ctx.cfg.rst_gpio_pin, 0);
}

static inline void st7789_rst_high(void)
{
    gpio_write_pin(st7789_ctx.cfg.gpio_base, st7789_ctx.cfg.rst_gpio_index, 
                   st7789_ctx.cfg.rst_gpio_pin, 1);
}

static void st7789_delay_ms(uint32_t ms)
{
    board_delay_ms(ms);
}

static inline void st7789_spi_wait_transfer_done(SPI_Type *spi)
{
    /* FIFO empty does NOT always mean the shifter is done.
     * Wait for both FIFO empty and SPI inactive. */
    while (spi_get_tx_fifo_valid_data_size(spi) != 0U) {
    }
    while (spi_is_active(spi)) {
    }
}

static void st7789_spi_write_byte(uint8_t data)
{
    SPI_Type *spi = st7789_ctx.cfg.spi_base;

    /* Configure this transfer (1 byte) */
    spi_set_write_data_count(spi, 1);
    
    /* Wait for TX FIFO not full */
    while (spi_get_tx_fifo_valid_data_size(spi) >= SPI_SOC_FIFO_DEPTH) {
    }
    
    spi->DATA = data;
    
    /* Wait for transfer complete (avoid missing a short SPIACTIVE pulse) */
    st7789_spi_wait_transfer_done(spi);
}

static void st7789_spi_write_data(const uint8_t *data, uint32_t len)
{
    SPI_Type *spi = st7789_ctx.cfg.spi_base;

    if ((data == NULL) || (len == 0U)) {
        return;
    }

    /* Configure this transfer (len bytes, SPI data length is 8-bit) */
    spi_set_write_data_count(spi, len);
    
    for (uint32_t i = 0; i < len; i++) {
        while (spi_get_tx_fifo_valid_data_size(spi) >= SPI_SOC_FIFO_DEPTH) {
        }
        spi->DATA = data[i];
    }
    
    /* Wait for all data sent */
    st7789_spi_wait_transfer_done(spi);
}

static void st7789_write_cmd(uint8_t cmd)
{
    st7789_dc_command();
    st7789_spi_write_byte(cmd);
}

static void st7789_write_data(uint8_t data)
{
    st7789_dc_data();
    st7789_spi_write_byte(data);
}

static void st7789_write_data_buf(const uint8_t *data, uint32_t len)
{
    st7789_dc_data();
    st7789_spi_write_data(data, len);
}

static void st7789_write_cmd_data_buf(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    st7789_write_cmd(cmd);
    if ((data != NULL) && (len != 0U)) {
        st7789_write_data_buf(data, len);
    }
}

/*============================================================================
 * Initialization sequences
 *============================================================================*/
static void st7789_init_sequence(void)
{
    static const uint8_t porctrl[] = { 0x0C, 0x0C, 0x00, 0x33, 0x33 };
    static const uint8_t pwctrl1[] = { 0xA4, 0xA1 };
    static const uint8_t gamma_pos[] = {
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
        0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23
    };
    static const uint8_t gamma_neg[] = {
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
        0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23
    };

    /* Software reset */
    st7789_write_cmd(ST7789_SWRESET);
    st7789_delay_ms(150);
    
    /* Sleep out */
    st7789_write_cmd(ST7789_SLPOUT);
    st7789_delay_ms(120);
    
    /* Color mode - RGB565 */
    st7789_write_cmd_data_buf(ST7789_COLMOD, (const uint8_t[]){0x55}, 1); /* 16-bit color */
    
    /* Memory data access control */
    st7789_write_cmd_data_buf(ST7789_MADCTL, (const uint8_t[]){0x00}, 1);
    
    /* Porch control */
    st7789_write_cmd_data_buf(ST7789_PORCTRL, porctrl, sizeof(porctrl));
    
    /* Gate control */
    st7789_write_cmd_data_buf(ST7789_GCTRL, (const uint8_t[]){0x35}, 1);
    
    /* VCOM setting */
    st7789_write_cmd_data_buf(ST7789_VCOMS, (const uint8_t[]){0x19}, 1);
    
    /* LCM control */
    st7789_write_cmd_data_buf(ST7789_LCMCTRL, (const uint8_t[]){0x2C}, 1);
    
    /* VDV and VRH command enable */
    st7789_write_cmd_data_buf(ST7789_VDVVRHEN, (const uint8_t[]){0x01}, 1);
    
    /* VRH set */
    st7789_write_cmd_data_buf(ST7789_VRHS, (const uint8_t[]){0x12}, 1);
    
    /* VDV set */
    st7789_write_cmd_data_buf(ST7789_VDVS, (const uint8_t[]){0x20}, 1);
    
    /* Frame rate control */
    st7789_write_cmd_data_buf(ST7789_FRCTRL2, (const uint8_t[]){0x0F}, 1);  /* 60Hz */
    
    /* Power control */
    st7789_write_cmd_data_buf(ST7789_PWCTRL1, pwctrl1, sizeof(pwctrl1));
    
    /* Positive voltage gamma control */
    st7789_write_cmd_data_buf(ST7789_PVGAMCTRL, gamma_pos, sizeof(gamma_pos));
    
    /* Negative voltage gamma control */
    st7789_write_cmd_data_buf(ST7789_NVGAMCTRL, gamma_neg, sizeof(gamma_neg));
    
    /* Inversion on (most ST7789 displays need this) */
    if (st7789_ctx.cfg.invert_colors) {
        st7789_write_cmd(ST7789_INVON);
    } else {
        st7789_write_cmd(ST7789_INVOFF);
    }
    
    /* Normal display mode on */
    st7789_write_cmd(ST7789_NORON);
    st7789_delay_ms(10);
    
    /* Display on */
    st7789_write_cmd(ST7789_DISPON);
    st7789_delay_ms(10);
}

static void gc9307_init_sequence(void)
{
    /* GC9307 is largely compatible with ST7789 */
    /* Use ST7789 init sequence with minor adjustments if needed */
    st7789_init_sequence();
    
    /* GC9307 specific settings can be added here */
}

/*============================================================================
 * GPIO initialization
 *============================================================================*/
static void st7789_gpio_init(void)
{
    /* D/C pin */
    gpio_set_pin_output(st7789_ctx.cfg.gpio_base, 
                        st7789_ctx.cfg.dc_gpio_index,
                        st7789_ctx.cfg.dc_gpio_pin);
    
    /* RST pin */
    gpio_set_pin_output(st7789_ctx.cfg.gpio_base, 
                        st7789_ctx.cfg.rst_gpio_index,
                        st7789_ctx.cfg.rst_gpio_pin);
    
    /* Backlight pin */
    gpio_set_pin_output(st7789_ctx.cfg.gpio_base, 
                        st7789_ctx.cfg.bl_gpio_index,
                        st7789_ctx.cfg.bl_gpio_pin);
}

/*============================================================================
 * SPI initialization
 *============================================================================*/
static hpm_stat_t st7789_spi_init(void)
{
    spi_timing_config_t timing = {0};
    spi_format_config_t format = {0};
    spi_control_config_t control = {0};
    SPI_Type *spi = st7789_ctx.cfg.spi_base;
    uint32_t spi_clk;
    
    /* Enable SPI clock */
    clock_add_to_group(st7789_ctx.cfg.spi_clk_name, 0);
    
    /* Get SPI clock */
    spi_clk = clock_get_frequency(st7789_ctx.cfg.spi_clk_name);
    
    /* Configure timing */
    spi_master_get_default_timing_config(&timing);
    timing.master_config.clk_src_freq_in_hz = spi_clk;
    timing.master_config.sclk_freq_in_hz = st7789_ctx.cfg.spi_freq_hz;
    timing.master_config.cs2sclk = spi_cs2sclk_half_sclk_1;
    timing.master_config.csht = spi_csht_half_sclk_1;
    
    if (spi_master_timing_init(spi, &timing) != status_success) {
        return status_fail;
    }
    
    /* Configure format - 8-bit, MSB first, Mode 0 */
    spi_master_get_default_format_config(&format);
    format.master_config.addr_len_in_bytes = 0;
    format.common_config.data_len_in_bits = 8;
    format.common_config.data_merge = false;
    format.common_config.mosi_bidir = false;
    format.common_config.lsb = false;
    format.common_config.mode = spi_master_mode;
    format.common_config.cpol = spi_sclk_low_idle;
    format.common_config.cpha = spi_sclk_sampling_odd_clk_edges;
    
    spi_format_init(spi, &format);
    
    /* Configure control */
    spi_master_get_default_control_config(&control);
    control.master_config.cmd_enable = false;
    control.master_config.addr_enable = false;
    control.master_config.token_enable = false;
    control.common_config.tx_dma_enable = false;  /* Will enable for DMA transfers */
    control.common_config.rx_dma_enable = false;
    control.common_config.trans_mode = spi_trans_write_only;
    control.common_config.data_phase_fmt = spi_single_io_mode;
    control.common_config.dummy_cnt = spi_dummy_count_1;
    
    /* Use minimal non-zero counts; actual counts are set per transfer */
    if (spi_control_init(spi, &control, 1, 1) != status_success) {
        return status_fail;
    }
    
    return status_success;
}

/*============================================================================
 * DMA initialization
 *============================================================================*/
static void st7789_dma_init(void)
{
    DMA_Type *dma = st7789_ctx.cfg.dma_base;
    DMAMUX_Type *dmamux = st7789_ctx.cfg.dmamux_base;
    uint8_t ch = st7789_ctx.cfg.dma_channel;
    uint8_t mux_ch = st7789_ctx.cfg.dma_mux_channel;

#ifdef DMA_SOC_CHN_TO_DMAMUX_CHN
    /* Avoid mismatch between DMA channel and DMAMUX channel */
    mux_ch = (uint8_t)DMA_SOC_CHN_TO_DMAMUX_CHN(dma, ch);
#endif
    
    /* Configure DMAMUX */
    dmamux_config(dmamux, mux_ch, st7789_ctx.cfg.dma_src_request, true);

    /* Ensure channel is idle and status is clean */
    dma_disable_channel(dma, ch);
    dma_clear_transfer_status(dma, ch);
    
    /* Enable DMA channel interrupt - use TERMINAL_COUNT for DMAv2 */
    dma_enable_channel_interrupt(dma, ch, DMA_INTERRUPT_MASK_TERMINAL_COUNT);
    
    st7789_ctx.dma_busy = false;
}

/*============================================================================
 * Public API implementation
 *============================================================================*/

hpm_stat_t st7789_init(const st7789_config_t *config)
{
    if (config == NULL) {
        return status_invalid_argument;
    }
    
    /* Store configuration */
    memcpy(&st7789_ctx.cfg, config, sizeof(st7789_config_t));
    st7789_ctx.rotation = config->rotation;
    st7789_ctx.width = config->width;
    st7789_ctx.height = config->height;
    st7789_ctx.dma_busy = false;
    
    /* Initialize GPIO */
    st7789_gpio_init();
    
    /* Hardware reset */
    st7789_rst_high();
    st7789_delay_ms(10);
    st7789_rst_low();
    st7789_delay_ms(10);
    st7789_rst_high();
    st7789_delay_ms(120);
    
    /* Initialize SPI */
    if (st7789_spi_init() != status_success) {
        return status_fail;
    }
    
    /* Initialize DMA */
    st7789_dma_init();
    
    /* Initialize display */
    if (config->driver_ic == LCD_DRIVER_GC9307) {
        gc9307_init_sequence();
    } else {
        st7789_init_sequence();
    }
    
    /* Set initial rotation */
    st7789_set_rotation(config->rotation);
    
    /* Turn on backlight */
    st7789_backlight(true);
    
    return status_success;
}

void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint16_t x_start = x0 + st7789_ctx.cfg.x_offset;
    uint16_t x_end = x1 + st7789_ctx.cfg.x_offset;
    uint16_t y_start = y0 + st7789_ctx.cfg.y_offset;
    uint16_t y_end = y1 + st7789_ctx.cfg.y_offset;
    
    /* Column address set */
    const uint8_t caset[] = {
        (uint8_t)(x_start >> 8),
        (uint8_t)(x_start & 0xFF),
        (uint8_t)(x_end >> 8),
        (uint8_t)(x_end & 0xFF),
    };
    st7789_write_cmd_data_buf(ST7789_CASET, caset, sizeof(caset));
    
    /* Row address set */
    const uint8_t raset[] = {
        (uint8_t)(y_start >> 8),
        (uint8_t)(y_start & 0xFF),
        (uint8_t)(y_end >> 8),
        (uint8_t)(y_end & 0xFF),
    };
    st7789_write_cmd_data_buf(ST7789_RASET, raset, sizeof(raset));
    
    /* Write to RAM */
    st7789_write_cmd(ST7789_RAMWR);
}

void st7789_fill_area(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint32_t pixel_count = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);
    uint8_t color_hi = color >> 8;
    uint8_t color_lo = color & 0xFF;
    
    st7789_set_window(x0, y0, x1, y1);
    
    st7789_dc_data();
    for (uint32_t i = 0; i < pixel_count; i++) {
        st7789_spi_write_byte(color_hi);
        st7789_spi_write_byte(color_lo);
    }
}

void st7789_write_pixels(const uint16_t *data, uint32_t pixel_count)
{
    st7789_dc_data();
    
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t byte_count = pixel_count * 2;
    
    st7789_spi_write_data(ptr, byte_count);
}

hpm_stat_t st7789_write_pixels_dma(const void *data, uint32_t byte_len,
                                    st7789_dma_done_cb_t callback, void *user_data)
{
    if (st7789_ctx.dma_busy) {
        return status_fail;
    }
    
    DMA_Type *dma = st7789_ctx.cfg.dma_base;
    SPI_Type *spi = st7789_ctx.cfg.spi_base;
    uint8_t ch = st7789_ctx.cfg.dma_channel;
    dma_channel_config_t dma_cfg = {0};

    if ((data == NULL) || (byte_len == 0U)) {
        return status_invalid_argument;
    }
    
    /* Flush cache for DMA source buffer */
    if (l1c_dc_is_enabled()) {
        l1c_dc_writeback((uint32_t)data, byte_len);
    }
    
    /* Store callback */
    st7789_ctx.dma_callback = callback;
    st7789_ctx.dma_user_data = user_data;
    st7789_ctx.dma_busy = true;
    
    /* Set D/C to data mode */
    st7789_dc_data();

    /* Configure SPI transfer count (8-bit SPI, so count == bytes) */
    spi_set_write_data_count(spi, byte_len);

    /* Enable SPI TX DMA */
    spi_enable_tx_dma(spi);
    
    /* Configure DMA transfer */
    dma_default_channel_config(dma, &dma_cfg);
    dma_cfg.src_addr = core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)data);
    dma_cfg.dst_addr = core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)&spi->DATA);
    dma_cfg.src_width = DMA_TRANSFER_WIDTH_BYTE;
    dma_cfg.dst_width = DMA_TRANSFER_WIDTH_BYTE;
    dma_cfg.src_addr_ctrl = DMA_ADDRESS_CONTROL_INCREMENT;
    dma_cfg.dst_addr_ctrl = DMA_ADDRESS_CONTROL_FIXED;
    dma_cfg.size_in_byte = byte_len;
    dma_cfg.src_mode = DMA_HANDSHAKE_MODE_NORMAL;
    dma_cfg.dst_mode = DMA_HANDSHAKE_MODE_HANDSHAKE;
    
    /* Start DMA transfer */
    if (dma_setup_channel(dma, ch, &dma_cfg, true) != status_success) {
        st7789_ctx.dma_busy = false;
        spi_disable_tx_dma(spi);
        return status_fail;
    }
    
    return status_success;
}

bool st7789_is_busy(void)
{
    return st7789_ctx.dma_busy;
}

void st7789_wait_idle(void)
{
    while (st7789_ctx.dma_busy) {
        __asm volatile ("nop");
    }
}

void st7789_set_rotation(uint16_t rotation)
{
    uint8_t madctl = 0;
    
    st7789_ctx.rotation = (uint8_t)rotation;
    
    switch (rotation) {
    case 0:
        madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB;
        st7789_ctx.width = st7789_ctx.cfg.width;
        st7789_ctx.height = st7789_ctx.cfg.height;
        break;
    case 90:
        madctl = ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
        st7789_ctx.width = st7789_ctx.cfg.height;
        st7789_ctx.height = st7789_ctx.cfg.width;
        break;
    case 180:
        madctl = ST7789_MADCTL_RGB;
        st7789_ctx.width = st7789_ctx.cfg.width;
        st7789_ctx.height = st7789_ctx.cfg.height;
        break;
    case 270:
        madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
        st7789_ctx.width = st7789_ctx.cfg.height;
        st7789_ctx.height = st7789_ctx.cfg.width;
        break;
    default:
        madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB;
        break;
    }
    
    st7789_write_cmd(ST7789_MADCTL);
    st7789_write_data(madctl);
}

void st7789_display_on(bool on)
{
    if (on) {
        st7789_write_cmd(ST7789_DISPON);
    } else {
        st7789_write_cmd(ST7789_DISPOFF);
    }
}

void st7789_backlight(bool on)
{
    gpio_write_pin(st7789_ctx.cfg.gpio_base, 
                   st7789_ctx.cfg.bl_gpio_index,
                   st7789_ctx.cfg.bl_gpio_pin, 
                   on ? 1 : 0);
}

void st7789_invert(bool invert)
{
    if (invert) {
        st7789_write_cmd(ST7789_INVON);
    } else {
        st7789_write_cmd(ST7789_INVOFF);
    }
}

uint16_t st7789_get_width(void)
{
    return st7789_ctx.width;
}

uint16_t st7789_get_height(void)
{
    return st7789_ctx.height;
}

void st7789_dma_irq_handler(void)
{
    DMA_Type *dma = st7789_ctx.cfg.dma_base;
    SPI_Type *spi = st7789_ctx.cfg.spi_base;
    uint8_t ch = st7789_ctx.cfg.dma_channel;
    
    uint32_t stat = dma_check_transfer_status(dma, ch);

    /* Only handle terminal events; ignore ongoing/half-done */
    if ((stat & (DMA_CHANNEL_STATUS_TC | DMA_CHANNEL_STATUS_ERROR | DMA_CHANNEL_STATUS_ABORT)) == 0U) {
        return;
    }

    /* DMA TC only means FIFO writes are done; wait for SPI shifter to finish */
    if ((stat & DMA_CHANNEL_STATUS_TC) != 0U) {
        st7789_spi_wait_transfer_done(spi);
    }

    /* Stop DMA & mark idle */
    dma_disable_channel(dma, ch);
    spi_disable_tx_dma(spi);
    st7789_ctx.dma_busy = false;

    /* Always notify upper layer to avoid LVGL deadlock */
    if (st7789_ctx.dma_callback) {
        st7789_ctx.dma_callback(st7789_ctx.dma_user_data);
    }
}
