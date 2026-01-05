# HPM LVGL SPI Display Driver for ST7789/GC9307

[![License](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](LICENSE)
[![HPM SDK](https://img.shields.io/badge/HPM_SDK-1.11.0-green.svg)](https://github.com/hpmicro/hpm_sdk)
[![LVGL](https://img.shields.io/badge/LVGL-9.2.x-orange.svg)](https://github.com/lvgl/lvgl)

A high-performance LVGL display driver for ST7789/GC9307 SPI LCD on HPMicro HPM6E00 series MCUs. Optimized for 60FPS with DMA transfers and intelligent partial refresh.

## Features

- 🚀 **High Performance**: 40MHz SPI with DMA for smooth 60FPS animations
- 🔄 **Smart Partial Refresh**: Only transfers changed screen regions
- 📺 **Dual IC Support**: Works with both ST7789 and GC9307 driver ICs
- 🎨 **RGB565 Color**: 16-bit color depth for vibrant displays
- 💾 **Memory Efficient**: Configurable buffer size (default 1/4 screen)
- 🔧 **Easy Integration**: Drop-in component for HPM SDK projects

## Hardware Support

### Tested Platforms
- HPM6E00 FULL_PORT development board
- HPM6E80 EVK (should work with minor pin adjustments)

### Display Specifications
- Resolution: 172×320 (or other ST7789/GC9307 compatible)
- Interface: SPI (4-wire)
- Color: RGB565 (16-bit)
- Driver IC: ST7789 / GC9307

### Pin Configuration (HPM6E00 FULL_PORT)

| Signal | Pin | GPIO |
|--------|-----|------|
| SPI_SCLK | PF26 | SPI7_SCLK |
| SPI_MOSI | PF29 | SPI7_MOSI |
| SPI_CS | PF27 | SPI7_CS_0 |
| D/C | PF28 | GPIO_F_28 |
| RST | PF30 | GPIO_F_30 |
| BL | PF25 | GPIO_F_25 |

## Performance

### SPI Transfer Times (40MHz)
| Area Size | Data Size | Transfer Time |
|-----------|-----------|---------------|
| Full screen (172×320) | 110 KB | ~22 ms |
| 1/4 screen (172×80) | 27.5 KB | ~5.5 ms |
| Small widget (50×30) | 3 KB | ~0.6 ms |

### Frame Rate Estimates
- Static UI: No transfer needed
- Single widget update: >100 FPS
- Menu transition (1/4 screen): ~180 FPS
- Full screen animation: ~45 FPS

## Quick Start

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/hpm_lvgl_spi_st7789.git
```

### 2. Copy to Your Project

Copy the `src/` folder to your HPM SDK project:
```
your_project/
├── components/
│   └── lvgl_spi_display/    <- Copy src/* here
├── src/
│   └── main.c
└── CMakeLists.txt
```

### 3. Update CMakeLists.txt

```cmake
# Enable LVGL with custom porting
set(CONFIG_LVGL 1)
set(CONFIG_LVGL_CUSTOM_PORTABLE 1)

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(your_project)

# Add display driver
set(LVGL_SPI_DISPLAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/components/lvgl_spi_display)
sdk_inc(${LVGL_SPI_DISPLAY_DIR})
sdk_src(
    ${LVGL_SPI_DISPLAY_DIR}/st7789.c
    ${LVGL_SPI_DISPLAY_DIR}/hpm_lvgl_spi.c
)
sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG="lv_conf.h")
```

### 4. Initialize in main.c

```c
#include "hpm_lvgl_spi.h"

int main(void)
{
    board_init();
    board_init_lcd();    // Initialize LCD pins
    
    // Initialize LVGL + Display
    lv_display_t *disp = hpm_lvgl_spi_init();
    if (disp == NULL) {
        printf("Display init failed!\n");
        while(1);
    }
    
    // Create your UI
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello HPM!");
    lv_obj_center(label);
    
    // Main loop
    while (1) {
        lv_timer_handler();
        board_delay_ms(5);
    }
}
```

## API Reference

### Initialization

```c
lv_display_t *hpm_lvgl_spi_init(void);
```
Initialize LVGL and the SPI display. Returns display object or NULL on failure.

### Display Control

```c
void hpm_lvgl_spi_backlight(bool on);
void hpm_lvgl_spi_set_rotation(uint16_t rotation);  // 0, 90, 180, 270
```

### Utility

```c
uint32_t hpm_lvgl_spi_tick_get(void);   // Get current tick in ms
uint32_t hpm_lvgl_spi_get_fps(void);    // Get actual FPS
```

### Low-level ST7789 API

```c
hpm_stat_t st7789_init(const st7789_config_t *config);
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void st7789_fill_area(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
hpm_stat_t st7789_write_pixels_dma(const void *data, uint32_t byte_len,
                                    st7789_dma_done_cb_t callback, void *user_data);
```

## Configuration

### lv_conf.h Key Settings

```c
/* Display refresh period */
#define LV_DEF_REFR_PERIOD  16      // 16ms = ~60Hz

/* Dirty area tracking */
#define LV_INV_BUF_SIZE 32          // Max 32 independent dirty rectangles

/* Buffer size (in hpm_lvgl_spi.h) */
#define HPM_LVGL_FB_LINES  80       // 1/4 screen, ~55KB total with double buffer
```

### Custom Pin Configuration

Modify in `hpm_lvgl_spi.c`:

```c
/* Override board defaults if needed */
#define BOARD_LCD_SPI           HPM_SPI7
#define BOARD_LCD_DMA           HPM_HDMA
#define BOARD_LCD_DMA_CH        0
```

## Examples

### TSN Dashboard Demo

A complete example showing:
- Multi-page navigation with buttons
- Real-time FPS display
- Animated progress bars
- Port status indicators

See `examples/tsn_dashboard/` for the full implementation.

## Directory Structure

```
hpm_lvgl_spi_st7789/
├── src/
│   ├── st7789.h           # ST7789/GC9307 driver header
│   ├── st7789.c           # Driver implementation
│   ├── hpm_lvgl_spi.h     # LVGL adapter header
│   ├── hpm_lvgl_spi.c     # LVGL adapter implementation
│   ├── lv_conf.h          # LVGL configuration
│   └── CMakeLists.txt     # Component build file
├── examples/
│   └── tsn_dashboard/     # Demo application
├── docs/
│   ├── HARDWARE.md        # Hardware setup guide
│   └── PORTING.md         # Porting to other boards
├── README.md
└── LICENSE
```

## Requirements

- HPM SDK 1.11.0 or later
- LVGL 9.2.x (included in HPM SDK)
- GCC RISC-V toolchain

## Known Limitations

1. **No QSPI Support**: Standard 4-wire SPI only (ST7789/GC9307 don't support QSPI)
2. **HPM6E00 Specific**: DMAv2 driver used; may need adaptation for other HPM series
3. **No Touch Support**: Display-only driver; add touch separately if needed

## Troubleshooting

### Display shows nothing
- Check pin connections (especially D/C and RST)
- Verify SPI clock is enabled in `board_init_clock()`
- Check backlight polarity

### Slow frame rate
- Reduce `HPM_LVGL_FB_LINES` for faster partial updates
- Check if animations are causing full-screen redraws
- Use `hpm_lvgl_spi_get_fps()` to monitor actual FPS

### DMA transfer errors
- Ensure buffer is in non-cacheable memory section
- Check DMA channel conflicts with other peripherals

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Submit a pull request

## License

BSD 3-Clause License. See [LICENSE](LICENSE) for details.

## Acknowledgments

- [HPMicro](https://www.hpmicro.com/) for the HPM SDK
- [LVGL](https://lvgl.io/) for the graphics library
- Community contributors

## Contact

For questions and support, please open an issue on GitHub.
