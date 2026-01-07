# HPM LVGL SPI Display Driver (ST7789 / GC9307)

High-performance LVGL display driver for ST7789/GC9307 SPI LCDs on HPMicro HPM6E00 series MCUs.
This driver uses SPI + DMA (DMAv2 on HPM6E) and is optimized for LVGL partial refresh.

- HPM SDK: 1.11.0+ (tested)
- LVGL: v9.x (via HPM SDK)

## Features

- Asynchronous SPI TX DMA flush (non-blocking)
- LVGL partial rendering (`LV_DISPLAY_RENDER_MODE_PARTIAL`)
- ST7789 + GC9307 compatible init sequence
- Optional double buffering
- FPS helper + flush statistics helpers

## Repository Layout

- `src/`: driver + LVGL adapter (`st7789.*`, `hpm_lvgl_spi.*`, `lv_conf.h`)
- `examples/`: demo apps (`tsn_dashboard`, `render_benchmark`)
- `docs/`: wiring + porting notes

## Quick Start (Integrate into an HPM SDK Project)

1. Copy `src/*` into your project, for example:

```
your_project/
  components/
    lvgl_spi_display/   (copy files from this repo's src/)
  src/
    main.c
  CMakeLists.txt
```

2. Add the component to your `CMakeLists.txt`:

```cmake
set(CONFIG_LVGL 1)
set(CONFIG_LVGL_CUSTOM_PORTABLE 1)

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

set(LVGL_SPI_DISPLAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/components/lvgl_spi_display)
sdk_inc(${LVGL_SPI_DISPLAY_DIR})
sdk_src(
  ${LVGL_SPI_DISPLAY_DIR}/st7789.c
  ${LVGL_SPI_DISPLAY_DIR}/hpm_lvgl_spi.c
)

# Use this repo's lv_conf.h (make sure lv_conf.h is in include path)
sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG=\"lv_conf.h\")
```

3. Provide board definitions (`BOARD_LCD_*`) and pinmux/GPIO init.
See `docs/HARDWARE.md` and `docs/PORTING.md`.

4. Initialize in `main.c`:

```c
board_init();
board_init_lcd(); /* configure SPI pins + LCD control GPIOs (D/C, RST, BL) */

if (hpm_lvgl_spi_init() == NULL) {
    while (1) { }
}

while (1) {
    lv_timer_handler();
    board_delay_us(1000);
}
```

## Build Examples

### HPM6E00 FULL_PORT (recommended)

The board `hpm6e00_full_port` is provided by the `hpm_apps` repository, not `hpm_sdk`.
To build examples for this board, pass `BOARD_SEARCH_PATH` to CMake.

Render benchmark:

```bash
cd examples/render_benchmark
mkdir build && cd build
cmake -GNinja -DBOARD=hpm6e00_full_port -DBOARD_SEARCH_PATH=<path-to-hpm_apps>/boards ..
ninja
```

TSN dashboard:

```bash
cd examples/tsn_dashboard
mkdir build && cd build
cmake -GNinja -DBOARD=hpm6e00_full_port -DBOARD_SEARCH_PATH=<path-to-hpm_apps>/boards ..
ninja
```

## Troubleshooting (HPM6E / DMAv2)

If LVGL appears to "hang" during the first flush on HPM6E:

- Ensure the DMA IRQ is enabled and `BOARD_LCD_DMA_IRQ` matches the DMA controller you use.
- DMAv2: `dma_check_transfer_status()` returns a `DMA_CHANNEL_STATUS_*` bitmask.
  Do **not** compare it with `status_success`.
- Do not configure SPI transfer count as 0. The HPM SPI driver uses `count - 1`.

See `src/st7789.c` for the fixed implementation.

Deep-dive notes (DMA completion vs SPI idle, cache coherency, etc.): `docs/HPM6E_DMA_SPI_ST7789.md`.

## License

BSD-3-Clause. See `LICENSE`.
