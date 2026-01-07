# WARP.md

This file provides a short overview for Warp (warp.dev) and other terminals when working with this repository.

## What this repo is

LVGL SPI display driver for ST7789/GC9307 on HPMicro HPM6E00 series MCUs.

Key files:

- `src/st7789.c`: SPI + DMA driver (ST7789/GC9307)
- `src/hpm_lvgl_spi.c`: LVGL adapter (flush callback, buffers, tick)
- `docs/HPM6E_DMA_SPI_ST7789.md`: DMA-SPI deep dive notes (HPM6E)
- `examples/`: demo applications

## Build (examples)

This repo uses the HPM SDK CMake framework.
Make sure `HPM_SDK_BASE` and the RISC-V toolchain environment variables are set (or use `sdk_env/start_cmd.cmd`).

For `hpm6e00_full_port` (board definitions are in `hpm_apps`), pass `BOARD_SEARCH_PATH`:

```bash
cd examples/render_benchmark
mkdir build && cd build
cmake -GNinja -DBOARD=hpm6e00_full_port -DBOARD_SEARCH_PATH=<path-to-hpm_apps>/boards ..
ninja
```

```bash
cd examples/tsn_dashboard
mkdir build && cd build
cmake -GNinja -DBOARD=hpm6e00_full_port -DBOARD_SEARCH_PATH=<path-to-hpm_apps>/boards ..
ninja
```
