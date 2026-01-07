# Official Backend: LVGL `lv_st7789` + HPM SDK `hpm_spi` + `dma_mgr`

This repository supports an "official" integration path that matches upstream HPM SDK patterns:

- LVGL upstream display driver: `lv_st7789` (wrapper around generic MIPI DBI/DCS)
- HPM SDK SPI component: `components/spi` (`hpm_spi.h`)
- HPM SDK DMA manager: `components/dma_mgr` (`hpm_dma_mgr.h`)

The legacy `src/st7789.c` DMAv2 driver remains available as a fallback when DMA manager is not enabled.

## Why this matters

- DMA manager owns `IRQn_HDMA`/`IRQn_XDMA` ISRs, so applications should not register another ISR for those IRQs.
- LVGL upstream driver expects the platform to provide:
  - `send_cmd_cb`: usually polling transfer for controller commands
  - `send_color_cb`: usually DMA transfer for pixel data, and must call `lv_display_flush_ready()`

## Requirements

- HPM SDK with:
  - `components/spi` (hpm_spi)
  - `components/dma_mgr` (dma manager)
  - LVGL drivers under `middleware/lvgl/lvgl/src/drivers/display/st7789` and `.../lcd/lv_lcd_generic_mipi`

## Enable in CMake

Before `find_package(hpm-sdk ...)`:

```cmake
set(CONFIG_LVGL 1)
set(CONFIG_LVGL_CUSTOM_PORTABLE 1)

set(CONFIG_HPM_SPI 1)
set(CONFIG_DMA_MGR 1)
```

Enable LVGL drivers via extra config:

```cmake
sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG="lv_conf_ext.h")
```

## How it works (implementation notes)

In `src/hpm_lvgl_spi.c` when `USE_DMA_MGR=1`:

- Create display with `lv_st7789_create(...)`
- `send_cmd_cb`:
  - Assert CS (optional GPIO CS)
  - D/C low, send command bytes via `hpm_spi_transmit_blocking`
  - D/C high, send parameters via `hpm_spi_transmit_blocking`
  - Wait for SPI idle, deassert CS
- `send_color_cb`:
  - Assert CS
  - D/C low, send `RAMWR` via `hpm_spi_transmit_blocking`
  - D-cache writeback (if enabled)
  - D/C high, start DMA via `hpm_spi_transmit_nonblocking`
  - On DMA manager TC callback:
    - **wait for SPI to become idle** (`spi_is_active == false` and TX FIFO empty)
    - deassert CS
    - call `lv_display_flush_ready(disp)`

## Optional GPIO CS

If you want to manually control CS (recommended when sharing the SPI bus), define in your board:

```c
#define BOARD_LCD_CS_INDEX        GPIO_DO_GPIOF
#define BOARD_LCD_CS_PIN          27
/* Optional: default is active-low */
#define BOARD_LCD_CS_ACTIVE_LEVEL 0
```

If you use SPI hardware CS or tie CS low, you can omit these macros.

