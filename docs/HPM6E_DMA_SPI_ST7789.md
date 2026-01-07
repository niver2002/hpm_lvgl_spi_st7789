# HPM6E + ST7789：SPI + DMA 刷屏一次性移植指南（含 LVGL）

本文目标：把“能跑”升级到“**一次性 Port 成功 + 稳定跑**”。适用于 HPM6E（DMAv2/HDMA）+ ST7789（含 172×320 小屏）这类 **SPI LCD** 场景。

> 重要澄清：这里的 “MIPI” 仅指 **MIPI DBI/DCS 指令集**（`CASET/RASET/RAMWR/...`），不是 MIPI DSI 物理层外设。HPM6E 没有 DSI 外设不影响用 SPI + DMA 驱动 ST7789。

---

## 结论（可行性）

HPM6E（DMAv2/HDMA）可以在 ST7789 这类 SPI LCD 上稳定跑 **SPI TX + DMA** 刷屏（已被上游示例验证）：

- `hpm-hal`（Rust/Embassy）在 `hpm6e00evk` 上的 `spi_st7789_async.rs`：SPI + DMA 异步刷屏
- HPM SDK（C）`samples/spi_components`：SPI DMA 示例明确提醒 “DMA complete != SPI 完成，需要等 `spi_is_active()`”

本仓库的实现已经把这些关键点固化（等待 SPI idle、缓存一致性、避免 DMA IRQ 冲突等）。

---

## 推荐实现路径（两种后端，二选一）

### A. 官方路径（推荐，符合 HPM SDK “规范”）

适用于：你使用 HPM SDK + LVGL，并希望尽量贴近官方组件使用方式。

- LVGL upstream：`lv_st7789`（wrapper）+ `lv_lcd_generic_mipi`（通用 DCS/MIPI DBI 驱动）
- HPM SDK：`components/spi`（`hpm_spi`）+ `components/dma_mgr`（DMA manager）

优点：

- DMA manager 统一管理 DMA 资源与中断（避免重复注册 ISR）
- SPI 传输由 `hpm_spi_*` 组件完成，更接近官方 sample

注意：DMA manager 会提供 `dma0_isr`/`dma1_isr`（`IRQn_HDMA`/`IRQn_XDMA`），**你的工程里不要再定义同名 ISR**，否则链接冲突。

参考：`docs/OFFICIAL_HPM_SDK_BACKEND.md`。

### B. 传统路径（最小依赖 / 便于单文件拷贝）

适用于：你不想引入 `dma_mgr`，或你在较老 SDK 上，或你希望直接控制 DMAv2 参数。

- 本仓库：`src/st7789.c`（DMAv2 直配）+ `src/hpm_lvgl_spi.c`（自定义 flush）

注意：该路径需要你自己保证 DMA IRQ 正确启用并处理中断（或使用你 BSP 的中断注册方式）。

---

## 一次性 Port Checklist（按顺序做，基本就能跑）

### 1) 硬件连线与电气

- 确认供电：ST7789 模块一般 3.3V（部分模块板载 LDO 可 5V，但 IO 仍需 3.3V）
- SPI 线尽量短、共地良好；长杜邦线建议先降到 10~20MHz 验证
- 必需控制脚：`D/C`、`RST`、`BL`（背光可先拉高常亮）
- `CS`：可用硬件 CS、GPIO CS 或直接接地（单设备时常见）

参考接线与 pinmux：`docs/HARDWARE.md`。

### 2) Pinmux / GPIO 初始化（最常见失败点）

在 `board_init_lcd()` 里做：

- SPI：`SCLK`、`MOSI`（可选 `CS`）配置为 SPI 功能
- 控制脚：`D/C`、`RST`、`BL`（可选 GPIO CS）配置为 GPIO 输出，并给初始电平

如果你是 **GPIO CS**：确保 pad 配成 GPIO，而不是 SPI CS 复用功能。

### 3) 板级宏（board.h）最小集合

至少需要（两种后端都要）：

```c
#define BOARD_LCD_SPI          HPM_SPI7
#define BOARD_LCD_SPI_CLK_NAME clock_spi7

#define BOARD_LCD_GPIO         HPM_GPIO0
#define BOARD_LCD_D_C_INDEX    GPIO_DO_GPIOF
#define BOARD_LCD_D_C_PIN      28
#define BOARD_LCD_RESET_INDEX  GPIO_DO_GPIOF
#define BOARD_LCD_RESET_PIN    30
#define BOARD_LCD_BL_INDEX     GPIO_DO_GPIOF
#define BOARD_LCD_BL_PIN       25

/* 172x320 常见偏移（不同面板可能不同） */
#define BOARD_LCD_X_OFFSET     34
#define BOARD_LCD_Y_OFFSET     0
```

如果用 **GPIO CS（官方路径支持）**：

```c
#define BOARD_LCD_CS_INDEX        GPIO_DO_GPIOF
#define BOARD_LCD_CS_PIN          27
#define BOARD_LCD_CS_ACTIVE_LEVEL 0   /* 默认 0：低电平有效 */
```

如果用 **传统路径（DMAv2 直配）**，还需要 DMA 宏：

```c
#define BOARD_LCD_DMA        HPM_HDMA
#define BOARD_LCD_DMAMUX     HPM_DMAMUX
#define BOARD_LCD_DMA_CH     0
#define BOARD_LCD_DMA_SRC    HPM_DMA_SRC_SPI7_TX
#define BOARD_LCD_DMA_IRQ    IRQn_HDMA
```

### 4) 选择后端 + LVGL 配置（HPM SDK 工程）

**官方路径（推荐）**：在 app 的 `CMakeLists.txt` 里（`find_package(hpm-sdk ...)` 之前）启用：

```cmake
set(CONFIG_HPM_SPI 1)
set(CONFIG_DMA_MGR 1)
```

并启用 LVGL 的 ST7789 + generic MIPI：

```cmake
sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG="lv_conf_ext.h")
```

> 本仓库的 `src/lv_conf_ext.h` 专门为 HPM SDK 的 `CONFIG_LV_HAS_EXTRA_CONFIG` 设计（不会踩 `LV_CONF_H` include-guard）。

### 5) DMA IRQ / ISR（非常关键）

#### 官方路径

- **不要**自己再注册 `IRQn_HDMA`/`IRQn_XDMA` 的 ISR
- DMA manager 已提供 `dma0_isr` / `dma1_isr` 并在内部派发回调

#### 传统路径

- 你必须保证：DMA IRQ 已使能、IRQ 号正确、ISR 里调用 `st7789_dma_irq_handler()`（或 `hpm_lvgl_spi_dma_irq_handler()`）
- DMAv2 的 `dma_check_transfer_status()` 返回 **位掩码**（`DMA_CHANNEL_STATUS_TC/ERROR/ABORT`），不要拿它和 `status_success` 做相等比较

### 6) DMA TC != SPI 完成（必须等 SPI 空闲）

在 SPI TX DMA 模式下：

- DMA TC 只说明 DMA 已经把数据写进 SPI FIFO/寄存器（通过握手）
- SPI 末尾仍可能在移位输出，特别是 FIFO 里还有残留字节

因此：**在 DMA TC 回调/中断里必须等待 SPI 彻底空闲**，至少包含两步：

- TX FIFO 为空：`spi_get_tx_fifo_valid_data_size(spi) == 0`
- SPI 不 active：`spi_is_active(spi) == false`

本仓库在两条路径都做了这个等待，避免 “提前释放 CS / 提前 `flush_ready` → 画面异常或后续传输串扰”。

### 7) D-Cache 缓存一致性（花屏/错色的核心原因之一）

DMA 从内存读像素数据：如果源 buffer 在 cacheable RAM：

- CPU 写了像素但没写回 → DMA 读旧数据 → 花屏/错色/撕裂

建议二选一（也可以叠加）：

1. **把 LVGL draw buffer 放到 non-cacheable 段**（最省心）
2. **每次 DMA 前对源 buffer 做 D-cache writeback**（需按 cacheline 对齐起止地址）

本仓库默认：

- LVGL draw buffer：`HPM_LVGL_FB_ATTR` 放到 `.noncacheable`，并 `aligned(64)`（HPM6E D-cache line 为 64B）
- 若 D-cache 开启仍使用 cacheable buffer：会在 DMA 前做 `l1c_dc_writeback(...)`

### 8) SPI 频率与稳定性（先保成功，再追 60FPS）

建议步骤：

1. 首次点亮：先用 `10~20MHz`
2. 线短/阻抗好再提升到 `40MHz`（本仓库默认）
3. 若出现偶发花屏：先降频，再检查线长、地、IO 驱动强度、屏模块供电

### 9) 屏参数：offset / invert / RGB-BGR / rotation

172×320（常见 1.47"）面板往往需要：

- `x_offset=34`（`BOARD_LCD_X_OFFSET`）
- `invert=1`（INVON）

颜色异常时按顺序排查：

1. 先试 `invert`（`HPM_LVGL_LCD_INVERT` 或 `st7789_invert()`）
2. 再试 RGB/BGR（官方路径用 `HPM_LVGL_LCD_FLAGS |= LV_LCD_FLAG_BGR`）
3. 最后再怀疑像素字节序（见下一节）

旋转：

- 官方路径：用 `lv_display_set_rotation(disp, ...)`，generic MIPI 会更新 MADCTL
- 传统路径：用 `st7789_set_rotation()`

### 10) 像素字节序（RGB565 高低字节）

多数 ST7789 在 SPI 下接收 RGB565 时是 **高字节先传**（Big-Endian 语义）。

如果你看到的现象是：

- 颜色“完全不对”，不像简单 RGB/BGR 颠倒（比如纯红变成诡异颜色）

那么很可能是 **像素 16-bit 的高低字节顺序**不一致。

建议做一个纯色测试（红/绿/蓝/白/黑）快速判定。

可选解决思路（按侵入性从低到高）：

1. **确认 LVGL 的像素缓冲输出是否已经是 big-endian**（有些工程会在渲染或 flush 前做 swap）
2. 在驱动发送路径对像素做字节交换（代价是 CPU 时间）
3. 像素阶段切换 SPI 为 16-bit 传输（命令仍用 8-bit），让 SPI 以 MSB-first 发送 16-bit word（更高效，但需要你确认 SPI 驱动支持快速切换）

`hpm-hal` 的示例使用 `RawU16<BigEndian>` 明确表明其 framebuffer 采用 big-endian 语义，可作为对照。

### 11) 主循环 / tick（LVGL）

- baremetal：循环里 `lv_timer_handler()` + 小延时（1ms）
- tick 源：本仓库默认用 `MCHTMR` 做 `lv_tick_set_cb()`，无需额外 `lv_tick_inc()`
- RTOS：注意 LVGL 的线程/锁要求；若你在 ISR 回调里调用 `lv_display_flush_ready()`，确保你的 LVGL 端口允许（多数移植允许这么做，但不同 OS 配置可能要求 defer）

---

## 常见故障 → 快速定位

### 1) 屏全白/全黑/没反应

- 先确认 `RST` 拉动确实发生（波形/逻辑分析）
- 确认 `D/C` 在命令阶段为 0、数据阶段为 1
- `CS` 若为 GPIO：确认逻辑有效电平（很多模块 CS 低有效）
- 先把 SPI 降到 10MHz，并确认 `SCLK/MOSI` 波形干净

### 2) 第一次 flush “卡死不返回”

典型原因：

- DMA IRQ 没开或 IRQ 号不对（传统路径）
- DMAv2 状态判断写错（把 bitmask 当 status code）
- DMA TC 回调里忘了等 `spi_is_active==false`，导致后续事务被打断/死锁

### 3) 偶发花屏/局部撕裂

- D-cache 没做 writeback（或没按 cacheline 对齐）
- SPI 频率太高 + 线太长
- DMA 回调里提前释放 CS（未等 SPI idle）

### 4) 链接报错：`dma0_isr`/`dma1_isr` 重定义

- 说明你启用了 `CONFIG_DMA_MGR`，同时你的工程或组件又注册了同 IRQ 的 ISR
- 解决：保留 DMA manager 的 ISR，删除/屏蔽你自己的 ISR

---

## 参考链接（上游示例）

- `hpm-hal`（Rust/Embassy）异步 ST7789 DMA SPI：
  - https://github.com/hpmicro/hpm-hal/blob/master/examples/hpm6e00evk/src/bin/spi_st7789_async.rs
- `hpm_sdk`（C）SPI DMA 示例（包含 `spi_is_active()` 的关键提醒）：
  - https://github.com/hpmicro/hpm_sdk/tree/main/samples/spi_components

## Official backend note

如果你启用 HPM SDK `components/spi` + `dma_mgr` 并使用 LVGL upstream `lv_st7789`，请阅读：`docs/OFFICIAL_HPM_SDK_BACKEND.md`。
