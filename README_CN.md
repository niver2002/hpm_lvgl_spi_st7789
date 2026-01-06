# HPM LVGL SPI 显示驱动（ST7789 / GC9307）

适用于先楫 HPM6E00 系列 MCU 的 LVGL SPI LCD 显示驱动，支持 ST7789/GC9307。
驱动使用 SPI + DMA（HPM6E 为 DMAv2），并针对 LVGL 局部刷新做了优化。

- HPM SDK：1.11.0+（已验证可编译）
- LVGL：v9.x（使用 HPM SDK 内置 LVGL）

## 特性

- SPI TX DMA 异步刷新（非阻塞）
- LVGL 局部渲染（`LV_DISPLAY_RENDER_MODE_PARTIAL`）
- ST7789 + GC9307 初始化序列（兼容）
- 可选双缓冲
- FPS 统计 + flush 统计辅助接口

## 目录结构

- `src/`：底层驱动 + LVGL 适配层（`st7789.*`, `hpm_lvgl_spi.*`, `lv_conf.h`）
- `examples/`：示例程序（`tsn_dashboard`, `render_benchmark`）
- `docs/`：硬件连接与移植说明

## 快速集成（HPM SDK 工程）

1. 将本仓库 `src/*` 复制到你的工程，例如：

```
your_project/
  components/
    lvgl_spi_display/   （复制本仓库 src 下的文件）
  src/
    main.c
  CMakeLists.txt
```

2. 在工程 `CMakeLists.txt` 中加入组件：

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

sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG=\"lv_conf.h\")
```

3. 在你的板级工程中补齐 `BOARD_LCD_*` 宏定义，并完成 pinmux/GPIO 初始化。
参考 `docs/HARDWARE.md` 与 `docs/PORTING.md`。

4. 在 `main.c` 初始化：

```c
board_init();
board_init_lcd(); /* 配置 SPI 引脚 + LCD 控制 GPIO（D/C, RST, BL） */

if (hpm_lvgl_spi_init() == NULL) {
    while (1) { }
}

while (1) {
    lv_timer_handler();
    board_delay_us(1000);
}
```

## 构建示例

### HPM6E00 FULL_PORT（推荐）

`hpm6e00_full_port` 板级定义来自 `hpm_apps` 仓库（不在 `hpm_sdk` 内），因此需要在 CMake 里额外传入 `BOARD_SEARCH_PATH`。

Render benchmark：

```bash
cd examples/render_benchmark
mkdir build && cd build
cmake -GNinja -DBOARD=hpm6e00_full_port -DBOARD_SEARCH_PATH=<hpm_apps路径>/boards ..
ninja
```

TSN dashboard：

```bash
cd examples/tsn_dashboard
mkdir build && cd build
cmake -GNinja -DBOARD=hpm6e00_full_port -DBOARD_SEARCH_PATH=<hpm_apps路径>/boards ..
ninja
```

## 常见问题（HPM6E / DMAv2）

如果在 HPM6E 上 LVGL 初始化后第一次 flush “卡死/不返回”，通常是 DMA 中断或 DMAv2 状态判断不正确导致：

- 确认 DMA IRQ 已使能，且 `BOARD_LCD_DMA_IRQ` 与实际使用的 DMA 控制器一致。
- DMAv2 的 `dma_check_transfer_status()` 返回的是 `DMA_CHANNEL_STATUS_*` 位掩码，不能和 `status_success` 做相等比较。
- SPI 传输计数不能为 0，HPM SPI 驱动内部会使用 `count - 1`。

本仓库 `src/st7789.c` 已按上述要点修正。

## License

BSD-3-Clause，详见 `LICENSE`。

