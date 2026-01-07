# Hardware Setup Guide

This document describes how to connect an ST7789/GC9307 SPI LCD module to HPMicro HPM6E00 series boards.

## Signals (4-wire SPI + control)

Typical modules use:

- `VCC` / `GND`
- `SCLK` (SPI clock)
- `MOSI` (SPI data)
- `CS` (chip select)
- `D/C` (data/command select)
- `RST` (reset)
- `BL` (backlight, sometimes named `LED`/`BLK`)

## Example Wiring: HPM6E00 FULL_PORT (SPI7)

| LCD Pin | Function | HPM6E00 Pin | IOC Pad |
|--------:|----------|-------------|---------|
| VCC | 3.3V | 3.3V | - |
| GND | Ground | GND | - |
| SCLK | SPI clock | PF26 | IOC_PAD_PF26 |
| MOSI | SPI data | PF29 | IOC_PAD_PF29 |
| CS | SPI CS0 | PF27 | IOC_PAD_PF27 |
| D/C | Data/Command | PF28 | IOC_PAD_PF28 |
| RST | Reset | PF30 | IOC_PAD_PF30 |
| BL | Backlight | PF25 | IOC_PAD_PF25 |

## Pinmux Example

Your board pinmux should configure SPI and control GPIO pins, for example:

```c
void init_lcd_pins(void)
{
    /* SPI7 pins */
    HPM_IOC->PAD[IOC_PAD_PF26].FUNC_CTL = IOC_PF26_FUNC_CTL_SPI7_SCLK;
    HPM_IOC->PAD[IOC_PAD_PF27].FUNC_CTL = IOC_PF27_FUNC_CTL_SPI7_CS_0;
    HPM_IOC->PAD[IOC_PAD_PF29].FUNC_CTL = IOC_PF29_FUNC_CTL_SPI7_MOSI;

    /* LCD control pins as GPIO */
    HPM_IOC->PAD[IOC_PAD_PF25].FUNC_CTL = IOC_PF25_FUNC_CTL_GPIO_F_25;  /* BL */
    HPM_IOC->PAD[IOC_PAD_PF28].FUNC_CTL = IOC_PF28_FUNC_CTL_GPIO_F_28;  /* D/C */
    HPM_IOC->PAD[IOC_PAD_PF30].FUNC_CTL = IOC_PF30_FUNC_CTL_GPIO_F_30;  /* RST */
}
```

## About CS (HW CS vs GPIO CS)

- Many ST7789 modules allow tying `CS` low permanently (single device on bus).
- If you use SPI hardware CS (e.g. `SPI7_CS0`), this repo can work without any extra macros.
- If you want the driver to control CS as a normal GPIO (recommended when sharing the SPI bus),
  define `BOARD_LCD_CS_INDEX/BOARD_LCD_CS_PIN` in your board and configure the pad as GPIO in pinmux.

## GPIO Init Example (board.c)

```c
void board_init_lcd(void)
{
    init_lcd_pins();

    /* Backlight (active high on most modules) */
    gpio_set_pin_output_with_initial(HPM_GPIO0, GPIO_DO_GPIOF, 25, true);

    /* D/C */
    gpio_set_pin_output_with_initial(HPM_GPIO0, GPIO_DO_GPIOF, 28, false);

    /* Reset */
    gpio_set_pin_output_with_initial(HPM_GPIO0, GPIO_DO_GPIOF, 30, false);
}
```

## Display Offset Notes

Different LCD modules may require different RAM offsets:

- 172x320 (1.47"): `x_offset = 34`, `y_offset = 0` is common
- 240x320 (2.0"): `x_offset = 0`, `y_offset = 0`
- 240x240 (1.3"): `x_offset = 0`, `y_offset = 80` (or 0, depends on module)

## Troubleshooting Checklist

No display output:

- Check `VCC/GND`
- Verify reset sequence and `RST` wiring
- Verify `D/C` wiring
- Check SPI signals with a scope or logic analyzer
- Reduce SPI clock (e.g. 20 MHz) if you use long wires

Wrong colors:

- Try toggling inversion (`invert_colors` / `st7789_invert(true/false)`)
- Confirm color order setting (RGB/BGR)
