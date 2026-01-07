# HPM6E + ST7789：DMA SPI 可行性与实现要点

## 结论

HPM6E（DMAv2/HDMA）可以在 ST7789 这类 SPI LCD 上稳定跑 **SPI TX + DMA** 刷屏。
关键是把 “DMA 传输完成” 和 “SPI 总线真正移位完成” 这两件事区分开，并做好缓存一致性。

本仓库的 `src/st7789.c` / `src/hpm_lvgl_spi.c` 已按这些规则实现并用于 LVGL 的异步 flush。

## 为什么“DMA 完成 != SPI 发送完成”

在 HPM 的 SPI TX DMA 模式下，DMA 的 Terminal Count（TC）表示：

- DMA 已经把数据写入 SPI 的 FIFO/数据寄存器（通过握手）

但此时 SPI 末尾可能还有数据正在移位输出到 MOSI/SCLK 线上。

因此，**DMA TC 中断里需要再等 SPI 空闲**，典型写法就是等待：

- `spi_is_active(spi) == false`

这点在 HPM SDK 的 SPI DMA 示例里也会特别提醒（DMA complete 不代表 SPI 完成，需要等 `spi_is_active()`）。

本仓库在 `st7789_dma_irq_handler()` 中实现了这个等待，确保回调触发时总线已结束：

- DMA TC → 等 `spi_is_active == false` → 关闭 DMA/SPI TX DMA → 标记 idle → 调用回调（通知 LVGL `flush_ready`）

## 缓存一致性（D-Cache）

DMA 从内存读像素数据，如果源缓冲区在可缓存 RAM 中，可能出现：

- CPU 写了像素，但还在 D-Cache 里没写回内存
- DMA 读到旧数据 → 画面花屏/错色/撕裂

推荐做法二选一：

1. **把 LVGL draw buffer 放到 non-cacheable 段**（性能好、最省心）
2. **每次 DMA 前对源 buffer 做 D-Cache writeback**

本仓库两者都做了：

- `src/hpm_lvgl_spi.c` 默认把 LVGL 的 draw buffer 放进 `.noncacheable`（可用 `HPM_LVGL_FB_ATTR` 覆盖）
- `src/st7789.c` 在启动 DMA 前会检查 D-Cache 并对源 buffer 做 `l1c_dc_writeback()`

## 传输长度与分段

HPM6E 的 SPI 传输计数上限很大（对整帧/大块像素传输非常友好），通常不需要像旧系列那样按 `512` 这类上限分段。

如果你把代码移植到其他 HPM 系列，需要重新确认：

- `SPI_SOC_TRANSFER_COUNT_MAX` 的含义和上限
- DMA IP（DMAv2 vs DMA/PDMA）及 API 差异

## 和 LVGL 的对接方式（异步 flush）

LVGL 的 flush 回调里：

1. 发送窗口设置（`CASET/RASET`）+ `RAMWR`
2. 启动 SPI TX DMA（非阻塞返回）
3. DMA ISR 中调用 `lv_display_flush_ready()`

这样 LVGL 能继续做渲染，而 SPI 在后台通过 DMA 把像素推到屏幕上。

## 参考链接（上游示例）

- hpm-hal（Rust/Embassy）异步 ST7789 DMA SPI 示例：
  - https://github.com/hpmicro/hpm-hal/blob/master/examples/hpm6e00evk/src/bin/spi_st7789_async.rs
- hpm_sdk（C）SPI DMA 示例（注意 `spi_is_active()` 的等待说明）：
  - https://github.com/hpmicro/hpm_sdk/tree/main/samples/spi_components

