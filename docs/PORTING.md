# Porting Guide

This guide explains how to port the ST7789/GC9307 LVGL driver to other HPMicro boards or customize it for your hardware.

## Overview

The driver consists of two layers:
1. **st7789.c/h** - Low-level display driver (SPI + DMA)
2. **hpm_lvgl_spi.c/h** - LVGL integration layer

## Porting to Another HPM6E00 Board

### Step 1: Define Board-Specific Macros

Add these definitions to your `board.h`:

```c
/* LCD SPI configuration */
#define BOARD_LCD_SPI               HPM_SPI7        /* Your SPI instance */
#define BOARD_LCD_SPI_CLK_NAME      clock_spi7      /* Corresponding clock */

/* LCD GPIO pins */
#define BOARD_LCD_BL_CTRL           HPM_GPIO0
#define BOARD_LCD_BL_INDEX          GPIO_DO_GPIOF
#define BOARD_LCD_BL_PIN            25

#define BOARD_LCD_D_C_CTRL          HPM_GPIO0
#define BOARD_LCD_D_C_INDEX         GPIO_DO_GPIOF
#define BOARD_LCD_D_C_PIN           28

#define BOARD_LCD_RESET_CTRL        HPM_GPIO0
#define BOARD_LCD_RESET_INDEX       GPIO_DO_GPIOF
#define BOARD_LCD_RESET_PIN         30
```

### Step 2: Configure Pin Mux

In your `pinmux.c`, configure SPI and GPIO pins:

```c
void init_lcd_pins(void)
{
    /* SPI pins - adjust for your SPI port */
    HPM_IOC->PAD[IOC_PAD_PFxx].FUNC_CTL = IOC_PFxx_FUNC_CTL_SPIx_SCLK;
    HPM_IOC->PAD[IOC_PAD_PFxx].FUNC_CTL = IOC_PFxx_FUNC_CTL_SPIx_CS_0;
    HPM_IOC->PAD[IOC_PAD_PFxx].FUNC_CTL = IOC_PFxx_FUNC_CTL_SPIx_MOSI;

    /* GPIO pins for control signals */
    HPM_IOC->PAD[IOC_PAD_PFxx].FUNC_CTL = IOC_PFxx_FUNC_CTL_GPIO_F_xx;
    /* ... */
}
```

### Step 3: Initialize LCD in board.c

```c
void board_init_lcd(void)
{
    init_lcd_pins();
    
    gpio_set_pin_output_with_initial(BOARD_LCD_BL_CTRL, 
        BOARD_LCD_BL_INDEX, BOARD_LCD_BL_PIN, true);
    gpio_set_pin_output_with_initial(BOARD_LCD_D_C_CTRL, 
        BOARD_LCD_D_C_INDEX, BOARD_LCD_D_C_PIN, false);
    gpio_set_pin_output_with_initial(BOARD_LCD_RESET_CTRL, 
        BOARD_LCD_RESET_INDEX, BOARD_LCD_RESET_PIN, false);
}
```

## Porting to Other HPM Series (HPM5300, HPM6700, etc.)

### DMA Driver Differences

**HPM6E00 series** uses DMAv2:
```c
#include "hpm_dmav2_drv.h"
#define DMA_INTERRUPT_MASK_TC  DMA_INTERRUPT_MASK_TERMINAL_COUNT
```

**HPM6700/HPM5300 series** uses standard DMA:
```c
#include "hpm_dma_drv.h"
/* DMA_INTERRUPT_MASK_TC is already defined */
```

### Modify st7789.h

```c
/* Select DMA driver based on SOC */
#if defined(HPM6E00) || defined(HPM6E80)
    #include "hpm_dmav2_drv.h"
#else
    #include "hpm_dma_drv.h"
#endif
```

### DMA Request Sources

Different SoCs have different DMA request mappings. Check your SoC's header files:

```c
/* HPM6E00 */
#define BOARD_LCD_DMA_SRC   HPM_DMA_SRC_SPI7_TX

/* HPM6750 - example */
#define BOARD_LCD_DMA_SRC   HPM_DMA_SRC_SPI3_TX
```

## Customizing Display Resolution

### For Different Screen Sizes

Edit `hpm_lvgl_spi.h`:

```c
/* 172×320 (1.47" round corner) */
#define HPM_LVGL_LCD_WIDTH   172
#define HPM_LVGL_LCD_HEIGHT  320

/* 240×320 (2.0" standard) */
#define HPM_LVGL_LCD_WIDTH   240
#define HPM_LVGL_LCD_HEIGHT  320

/* 240×240 (1.3" square) */
#define HPM_LVGL_LCD_WIDTH   240
#define HPM_LVGL_LCD_HEIGHT  240
```

### Display Memory Offset

Different modules have different RAM offsets. Edit in `hpm_lvgl_spi.c`:

```c
#define BOARD_LCD_X_OFFSET  34   /* Adjust for your display */
#define BOARD_LCD_Y_OFFSET  0
```

Common offsets:
| Resolution | X Offset | Y Offset |
|------------|----------|----------|
| 172×320 | 34 | 0 |
| 240×320 | 0 | 0 |
| 240×240 | 0 | 0 or 80 |
| 135×240 | 52 | 40 |

## Adjusting SPI Speed

### Maximum Speeds
- ST7789: 70MHz theoretical, 40-50MHz practical
- GC9307: 60MHz theoretical, 30-40MHz practical

### Modify in hpm_lvgl_spi.h

```c
/* Conservative for long wires */
#define HPM_LVGL_SPI_FREQ   (20000000UL)  /* 20MHz */

/* Aggressive for short PCB traces */
#define HPM_LVGL_SPI_FREQ   (50000000UL)  /* 50MHz */
```

## Buffer Size Tuning

### Memory vs Performance Tradeoff

```c
/* Smaller buffer - less memory, more flush calls */
#define HPM_LVGL_FB_LINES   40   /* ~14KB × 2 = 28KB */

/* Larger buffer - more memory, fewer flush calls */
#define HPM_LVGL_FB_LINES   160  /* ~55KB × 2 = 110KB */

/* Full screen - maximum memory, single flush */
#define HPM_LVGL_FB_LINES   HPM_LVGL_LCD_HEIGHT
```

### Calculation
```
Buffer size = Width × Lines × 2 (RGB565) × 2 (double buffer)
Example: 172 × 80 × 2 × 2 = 55,040 bytes
```

## Using Different Display ICs

### GC9307 Specifics

GC9307 is mostly ST7789-compatible. The driver auto-detects:

```c
st7789_config_t cfg;
cfg.driver_ic = LCD_DRIVER_GC9307;  /* or LCD_DRIVER_ST7789 */
```

### Adding Support for Other ICs (ILI9341, etc.)

1. Add new IC enum in `st7789.h`:
```c
typedef enum {
    LCD_DRIVER_ST7789 = 0,
    LCD_DRIVER_GC9307,
    LCD_DRIVER_ILI9341,  /* New */
} lcd_driver_ic_t;
```

2. Add initialization sequence in `st7789.c`:
```c
static void ili9341_init_sequence(void)
{
    /* ILI9341 specific init commands */
    st7789_write_cmd(0x01);  /* Software reset */
    st7789_delay_ms(150);
    /* ... more commands ... */
}
```

3. Call in init:
```c
if (config->driver_ic == LCD_DRIVER_ILI9341) {
    ili9341_init_sequence();
}
```

## LVGL Configuration

### Memory Allocation

Edit `lv_conf.h`:

```c
/* Increase for complex UIs */
#define LV_MEM_SIZE (64 * 1024U)

/* Decrease for simple UIs */
#define LV_MEM_SIZE (32 * 1024U)
```

### Refresh Rate

```c
/* 60Hz target */
#define LV_DEF_REFR_PERIOD  16

/* 30Hz for power saving */
#define LV_DEF_REFR_PERIOD  33
```

### Dirty Area Optimization

```c
/* More areas for complex dashboards */
#define LV_INV_BUF_SIZE 64

/* Fewer for simple UIs */
#define LV_INV_BUF_SIZE 16
```

## Testing Your Port

### Basic Test

```c
int main(void)
{
    board_init();
    board_init_lcd();
    
    lv_display_t *disp = hpm_lvgl_spi_init();
    
    /* Fill screen red */
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFF0000), 0);
    
    while(1) {
        lv_timer_handler();
        printf("FPS: %lu\n", hpm_lvgl_spi_get_fps());
        board_delay_ms(1000);
    }
}
```

### Expected Results
- Screen should show solid red
- FPS should report ~60 for static screen
- No flickering or artifacts

## Common Porting Issues

### Issue: Display Initialization Fails
- Check SPI clock is enabled: `clock_add_to_group(clock_spi7, 0)`
- Verify reset timing (min 10ms low, 120ms after high)

### Issue: Wrong Orientation
- Adjust MADCTL in `st7789_set_rotation()`
- Try different combinations of MX, MY, MV bits

### Issue: Colors Inverted
- Toggle `cfg.invert_colors = true/false`
- Some displays need INVON, others need INVOFF

### Issue: DMA Transfer Hangs
- Check DMAMUX configuration
- Verify DMA request source matches SPI port
- Ensure buffer is in non-cacheable memory

## Integration with RTOS

### FreeRTOS Example

```c
void lvgl_task(void *pvParameters)
{
    hpm_lvgl_spi_init();
    create_ui();
    
    while(1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

### Thread Safety

LVGL is not thread-safe by default. Protect with mutex:

```c
static SemaphoreHandle_t lvgl_mutex;

void lvgl_lock(void) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
}

void lvgl_unlock(void) {
    xSemaphoreGive(lvgl_mutex);
}

/* In lv_conf.h */
#define LV_USE_OS LV_OS_FREERTOS
```
