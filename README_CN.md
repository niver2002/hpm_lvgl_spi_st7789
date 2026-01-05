# HPM LVGL SPI显示驱动 - ST7789/GC9307

[![License](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](LICENSE)
[![HPM SDK](https://img.shields.io/badge/HPM_SDK-1.11.0-green.svg)](https://github.com/hpmicro/hpm_sdk)
[![LVGL](https://img.shields.io/badge/LVGL-9.2.x-orange.svg)](https://github.com/lvgl/lvgl)

适用于先楫HPM6E00系列MCU的高性能LVGL SPI LCD驱动，支持ST7789/GC9307驱动IC，通过DMA传输和智能局部刷新优化，可达60FPS流畅动画效果。

## 特性

- 🚀 **高性能**：40MHz SPI + DMA，流畅60FPS动画
- 🔄 **智能局部刷新**：仅传输屏幕变化区域
- 📺 **双IC支持**：兼容ST7789和GC9307驱动芯片
- 🎨 **RGB565色彩**：16位真彩显示
- 💾 **内存高效**：可配置缓冲区大小（默认1/4屏幕）
- 🔧 **易于集成**：即插即用的HPM SDK组件

## 硬件支持

### 测试平台
- HPM6E00 FULL_PORT开发板
- HPM6E80 EVK（需微调引脚配置）

### 显示屏规格
- 分辨率：172×320（或其他ST7789/GC9307兼容屏）
- 接口：SPI（4线制）
- 色彩：RGB565（16位）
- 驱动IC：ST7789 / GC9307

### 引脚配置（HPM6E00 FULL_PORT）

| 信号 | 引脚 | GPIO |
|------|-----|------|
| SPI_SCLK | PF26 | SPI7_SCLK |
| SPI_MOSI | PF29 | SPI7_MOSI |
| SPI_CS | PF27 | SPI7_CS_0 |
| D/C | PF28 | GPIO_F_28 |
| RST | PF30 | GPIO_F_30 |
| BL | PF25 | GPIO_F_25 |

## 性能参数

### SPI传输时间（40MHz）
| 区域大小 | 数据量 | 传输时间 |
|---------|-------|---------|
| 全屏 (172×320) | 110 KB | ~22 ms |
| 1/4屏 (172×80) | 27.5 KB | ~5.5 ms |
| 小控件 (50×30) | 3 KB | ~0.6 ms |

### 帧率预估
- 静态界面：无需传输
- 单控件更新：>100 FPS
- 菜单切换（1/4屏）：~180 FPS
- 全屏动画：~45 FPS

## 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/yourusername/hpm_lvgl_spi_st7789.git
```

### 2. 复制到项目

将 `src/` 文件夹复制到您的HPM SDK项目：
```
your_project/
├── components/
│   └── lvgl_spi_display/    <- 复制src/*到这里
├── src/
│   └── main.c
└── CMakeLists.txt
```

### 3. 修改CMakeLists.txt

```cmake
# 启用LVGL并使用自定义porting
set(CONFIG_LVGL 1)
set(CONFIG_LVGL_CUSTOM_PORTABLE 1)

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(your_project)

# 添加显示驱动
set(LVGL_SPI_DISPLAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/components/lvgl_spi_display)
sdk_inc(${LVGL_SPI_DISPLAY_DIR})
sdk_src(
    ${LVGL_SPI_DISPLAY_DIR}/st7789.c
    ${LVGL_SPI_DISPLAY_DIR}/hpm_lvgl_spi.c
)
sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG="lv_conf.h")
```

### 4. 在main.c中初始化

```c
#include "hpm_lvgl_spi.h"

int main(void)
{
    board_init();
    board_init_lcd();    // 初始化LCD引脚
    
    // 初始化LVGL + 显示屏
    lv_display_t *disp = hpm_lvgl_spi_init();
    if (disp == NULL) {
        printf("显示初始化失败!\n");
        while(1);
    }
    
    // 创建UI
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "你好 HPM!");
    lv_obj_center(label);
    
    // 主循环
    while (1) {
        lv_timer_handler();
        board_delay_ms(5);
    }
}
```

## API参考

### 初始化

```c
lv_display_t *hpm_lvgl_spi_init(void);
```
初始化LVGL和SPI显示屏。成功返回显示对象，失败返回NULL。

### 显示控制

```c
void hpm_lvgl_spi_backlight(bool on);                    // 背光开关
void hpm_lvgl_spi_set_rotation(uint16_t rotation);       // 旋转：0, 90, 180, 270
```

### 工具函数

```c
uint32_t hpm_lvgl_spi_tick_get(void);   // 获取当前tick（毫秒）
uint32_t hpm_lvgl_spi_get_fps(void);    // 获取实际帧率
```

## 配置说明

### lv_conf.h关键配置

```c
/* 刷新周期 */
#define LV_DEF_REFR_PERIOD  16      // 16ms = ~60Hz

/* 脏区域追踪 */
#define LV_INV_BUF_SIZE 32          // 最多32个独立脏矩形

/* 缓冲区大小（在hpm_lvgl_spi.h中） */
#define HPM_LVGL_FB_LINES  80       // 1/4屏幕，双缓冲约55KB
```

## 示例程序

### TSN仪表盘Demo

完整示例展示：
- 按键控制的多页面导航
- 实时FPS显示
- 动态进度条
- 端口状态指示器

详见 `examples/tsn_dashboard/` 目录。

## 目录结构

```
hpm_lvgl_spi_st7789/
├── src/
│   ├── st7789.h           # ST7789/GC9307驱动头文件
│   ├── st7789.c           # 驱动实现
│   ├── hpm_lvgl_spi.h     # LVGL适配层头文件
│   ├── hpm_lvgl_spi.c     # LVGL适配层实现
│   ├── lv_conf.h          # LVGL配置
│   └── CMakeLists.txt     # 组件构建文件
├── examples/
│   └── tsn_dashboard/     # Demo应用
├── docs/
│   ├── HARDWARE.md        # 硬件连接指南
│   └── PORTING.md         # 移植指南
├── README.md              # 英文文档
├── README_CN.md           # 中文文档
└── LICENSE
```

## 环境要求

- HPM SDK 1.11.0或更高版本
- LVGL 9.2.x（HPM SDK已包含）
- GCC RISC-V工具链

## 已知限制

1. **不支持QSPI**：仅支持标准4线SPI（ST7789/GC9307不支持QSPI）
2. **HPM6E00专用**：使用DMAv2驱动，移植到其他HPM系列需适配
3. **不含触摸支持**：仅显示驱动，触摸需另外添加

## 故障排除

### 显示屏无输出
- 检查引脚连接（特别是D/C和RST）
- 确认SPI时钟已在`board_init_clock()`中使能
- 检查背光极性

### 帧率较低
- 减小`HPM_LVGL_FB_LINES`以加快局部刷新
- 检查动画是否导致全屏重绘
- 使用`hpm_lvgl_spi_get_fps()`监控实际帧率

### DMA传输错误
- 确保缓冲区在非缓存内存段
- 检查DMA通道是否与其他外设冲突

## 贡献

欢迎贡献代码！请：
1. Fork本仓库
2. 创建功能分支
3. 提交Pull Request

## 许可证

BSD 3-Clause许可证。详见 [LICENSE](LICENSE)。

## 致谢

- [先楫半导体](https://www.hpmicro.com/) 提供HPM SDK
- [LVGL](https://lvgl.io/) 图形库
- 社区贡献者

## 联系方式

如有问题，请在GitHub上提交Issue。
