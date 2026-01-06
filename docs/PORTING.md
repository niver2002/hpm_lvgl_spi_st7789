# Porting Guide

This guide explains how to port the ST7789/GC9307 LVGL driver to your HPMicro board.

## Overview

The driver is split into two layers:

1. `st7789.c/h`: low-level SPI + DMA driver (ST7789/GC9307)
2. `hpm_lvgl_spi.c/h`: LVGL display adapter (flush callback, buffers, tick, stats)

## Required Board Macros

The LVGL adapter (`src/hpm_lvgl_spi.c`) reads board configuration from `board.h`.
You can either provide these macros in your board support package, or override them via compiler definitions.

### SPI

```c
#define BOARD_LCD_SPI               HPM_SPI7
#define BOARD_LCD_SPI_CLK_NAME      clock_spi7
```

### GPIO (control pins)

All control pins are written through a single `GPIO_Type *` base:

```c
#define BOARD_LCD_GPIO              HPM_GPIO0

#define BOARD_LCD_D_C_INDEX         GPIO_DO_GPIOF
#define BOARD_LCD_D_C_PIN           28

#define BOARD_LCD_RESET_INDEX       GPIO_DO_GPIOF
#define BOARD_LCD_RESET_PIN         30

#define BOARD_LCD_BL_INDEX          GPIO_DO_GPIOF
#define BOARD_LCD_BL_PIN            25
```

### DMA (HPM6E00 / DMAv2)

Defaults are provided in `src/hpm_lvgl_spi.c` for typical HPM6E00 setups, but you can override:

```c
#define BOARD_LCD_DMA               HPM_HDMA
#define BOARD_LCD_DMAMUX            HPM_DMAMUX
#define BOARD_LCD_DMA_CH            0
#define BOARD_LCD_DMA_SRC           HPM_DMA_SRC_SPI7_TX
#define BOARD_LCD_DMA_IRQ           IRQn_HDMA
```

## Pinmux Checklist

You must configure:

- SPI pins (SCLK/MOSI/CS)
- Control pins as GPIO (BL/D-C/RST)

See `docs/HARDWARE.md` for an example.

## Build Notes (hpm_apps boards)

Some boards (e.g. `hpm6e00_full_port`) are defined in the `hpm_apps` repository instead of `hpm_sdk`.
In that case you must pass `BOARD_SEARCH_PATH` to CMake:

```bash
cmake -GNinja -DBOARD=hpm6e00_full_port -DBOARD_SEARCH_PATH=<path-to-hpm_apps>/boards ..
```

## Common Pitfalls (HPM6E / DMAv2)

If LVGL "hangs" during the first flush on HPM6E, check:

- DMA IRQ is enabled and the IRQ number matches the DMA controller you use.
- DMAv2: `dma_check_transfer_status()` returns a `DMA_CHANNEL_STATUS_*` bitmask (TC/ERROR/ABORT).
  Do **not** compare it to `status_success`.
- SPI transfer count must not be 0 (the SPI driver programs `count - 1` internally).

The current `src/st7789.c` implementation already follows these rules.

## Porting to Other HPM Series

Different SoCs use different DMA IP:

- HPM6E00/HPM6E80: DMAv2 (`hpm_dmav2_drv.h`)
- Some other series: standard DMA (`hpm_dma_drv.h`)

If you port to a non-DMAv2 SoC, re-check:

- DMA request source (`HPM_DMA_SRC_SPIx_TX`)
- DMA driver API differences
- Cache maintenance requirements (if buffers are cacheable)

