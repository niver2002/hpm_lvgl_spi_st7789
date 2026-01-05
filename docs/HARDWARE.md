# Hardware Setup Guide

This document describes how to connect ST7789/GC9307 SPI LCD displays to HPM6E00 series development boards.

## Display Module Requirements

### Supported Display ICs
- **ST7789**: Most common, max 70MHz SPI
- **GC9307**: Pin-compatible with ST7789, max 60MHz SPI, often cheaper

### Recommended Display Specs
- Resolution: 172×320, 240×240, 240×320
- Interface: 4-wire SPI (MOSI, SCLK, CS, D/C)
- Voltage: 3.3V logic
- Connector: FPC or header pins

### Common Display Modules
- 1.47" 172×320 ST7789 (round corners, common in smartwatches)
- 1.3" 240×240 ST7789 (square)
- 2.0" 240×320 ST7789

## HPM6E00 FULL_PORT Connection

### SPI7 Pins (Default Configuration)

| LCD Pin | Function | HPM6E00 Pin | IOC Pad |
|---------|----------|-------------|---------|
| VCC | Power | 3.3V | - |
| GND | Ground | GND | - |
| SCL/SCLK | SPI Clock | PF26 | IOC_PAD_PF26 |
| SDA/MOSI | SPI Data | PF29 | IOC_PAD_PF29 |
| CS | Chip Select | PF27 | IOC_PAD_PF27 |
| DC/RS | Data/Command | PF28 | IOC_PAD_PF28 |
| RST | Reset | PF30 | IOC_PAD_PF30 |
| BL/LED | Backlight | PF25 | IOC_PAD_PF25 |

### Pin Initialization Code

The board's `pinmux.c` should include:

```c
void init_lcd_pins(void)
{
    /* SPI7 pins */
    HPM_IOC->PAD[IOC_PAD_PF26].FUNC_CTL = IOC_PF26_FUNC_CTL_SPI7_SCLK;
    HPM_IOC->PAD[IOC_PAD_PF27].FUNC_CTL = IOC_PF27_FUNC_CTL_SPI7_CS_0;
    HPM_IOC->PAD[IOC_PAD_PF29].FUNC_CTL = IOC_PF29_FUNC_CTL_SPI7_MOSI;

    /* LCD control pins as GPIO */
    HPM_IOC->PAD[IOC_PAD_PF25].PAD_CTL = IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1);
    HPM_IOC->PAD[IOC_PAD_PF25].FUNC_CTL = IOC_PF25_FUNC_CTL_GPIO_F_25;  /* BL */
    
    HPM_IOC->PAD[IOC_PAD_PF28].PAD_CTL = IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1);
    HPM_IOC->PAD[IOC_PAD_PF28].FUNC_CTL = IOC_PF28_FUNC_CTL_GPIO_F_28;  /* D/C */
    
    HPM_IOC->PAD[IOC_PAD_PF30].PAD_CTL = IOC_PAD_PAD_CTL_PE_SET(1) | IOC_PAD_PAD_CTL_PS_SET(1);
    HPM_IOC->PAD[IOC_PAD_PF30].FUNC_CTL = IOC_PF30_FUNC_CTL_GPIO_F_30;  /* RST */
}
```

### GPIO Initialization in board.c

```c
void board_init_lcd(void)
{
    init_lcd_pins();
    
    /* Backlight - active high (check your display!) */
    gpio_set_pin_output_with_initial(HPM_GPIO0, GPIO_DO_GPIOF, 25, true);
    
    /* D/C pin */
    gpio_set_pin_output_with_initial(HPM_GPIO0, GPIO_DO_GPIOF, 28, false);
    
    /* Reset pin */
    gpio_set_pin_output_with_initial(HPM_GPIO0, GPIO_DO_GPIOF, 30, false);
}
```

## Wiring Diagram

```
HPM6E00 FULL_PORT                    ST7789 Display
    ┌────────────┐                   ┌────────────┐
    │            │                   │            │
    │     PF26 ──┼───── SCLK ────────┼── SCL      │
    │     PF29 ──┼───── MOSI ────────┼── SDA      │
    │     PF27 ──┼───── CS ──────────┼── CS       │
    │     PF28 ──┼───── DC ──────────┼── DC/RS    │
    │     PF30 ──┼───── RST ─────────┼── RES      │
    │     PF25 ──┼───── BL ──────────┼── BLK      │
    │            │                   │            │
    │     3.3V ──┼───────────────────┼── VCC      │
    │     GND ───┼───────────────────┼── GND      │
    │            │                   │            │
    └────────────┘                   └────────────┘
```

## Display Offset Configuration

Different display modules may require different memory offset values:

### 172×320 Display (1.47")
```c
#define ST7789_X_OFFSET  34
#define ST7789_Y_OFFSET  0
```

### 240×320 Display (2.0")
```c
#define ST7789_X_OFFSET  0
#define ST7789_Y_OFFSET  0
```

### 240×240 Display (1.3")
```c
#define ST7789_X_OFFSET  0
#define ST7789_Y_OFFSET  80  /* Or 0, depending on module */
```

## Backlight Control

### Active High Backlight (Most Common)
```c
void st7789_backlight(bool on)
{
    gpio_write_pin(gpio_base, bl_index, bl_pin, on ? 1 : 0);
}
```

### Active Low Backlight
```c
void st7789_backlight(bool on)
{
    gpio_write_pin(gpio_base, bl_index, bl_pin, on ? 0 : 1);
}
```

### PWM Brightness Control (Optional)
For brightness control, connect BL to a PWM-capable pin and use:
```c
/* Example using GPTMR PWM */
void st7789_set_brightness(uint8_t percent)
{
    uint32_t duty = (percent * PWM_RELOAD) / 100;
    gptmr_update_cmp_value(HPM_GPTMR0, 0, 0, duty);
}
```

## Power Considerations

### Current Requirements
- Display: ~20-50mA (varies with content)
- Backlight: ~20-100mA (depends on LED count)
- Total: ~50-150mA from 3.3V

### Decoupling
- Add 100nF ceramic capacitor near VCC pin
- Add 10µF electrolytic for bulk decoupling

## Troubleshooting

### No Display Output
1. Check power connections (VCC, GND)
2. Verify reset sequence timing
3. Check D/C pin connection
4. Measure SPI signals with oscilloscope

### Wrong Colors
1. Check color inversion setting: `st7789_invert(true/false)`
2. Verify MADCTL (0x36) register settings
3. Check if display is RGB or BGR

### Flickering
1. Ensure stable power supply
2. Check SPI signal integrity
3. Reduce SPI clock if using long wires

### Partial Display
1. Verify X/Y offset values
2. Check CASET/RASET commands
3. Ensure window size matches display resolution

## Alternative SPI Ports

If SPI7 is not available, the driver can be adapted to other SPI ports:

| SPI Port | SCLK | MOSI | CS |
|----------|------|------|-----|
| SPI0 | PA03 | PA05 | PA04 |
| SPI1 | PB03 | PB05 | PB04 |
| SPI2 | PC03 | PC05 | PC04 |
| ... | ... | ... | ... |

Update `board.h` accordingly:
```c
#define BOARD_LCD_SPI           HPM_SPI0
#define BOARD_LCD_SPI_CLK_NAME  clock_spi0
```
