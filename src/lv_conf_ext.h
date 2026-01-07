/**
 * @file lv_conf_ext.h
 *
 * LVGL configuration overrides for this repository when building with HPM SDK's
 * `middleware/lvgl/lv_conf.h`.
 *
 * Usage:
 *   sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG=\"lv_conf_ext.h\")
 *
 * Notes:
 * - This file must NOT use the `LV_CONF_H` include guard, because the HPM SDK
 *   `lv_conf.h` includes it after defining `LV_CONF_H`.
 */

#ifndef HPM_LVGL_SPI_LV_CONF_EXT_H
#define HPM_LVGL_SPI_LV_CONF_EXT_H

/*====================
 * Core settings
 *====================*/

/* RGB565 is the natural format for ST7789 in SPI mode. */
#ifdef LV_COLOR_DEPTH
#undef LV_COLOR_DEPTH
#endif
#define LV_COLOR_DEPTH 16

/* Reduce LVGL heap to fit small MCU RAM budgets. Adjust as needed for your UI. */
#ifdef LV_MEM_SIZE
#undef LV_MEM_SIZE
#endif
#define LV_MEM_SIZE (48 * 1024U)

/* 16ms ~= 60Hz, good default for SPI LCDs. */
#ifdef LV_DEF_REFR_PERIOD
#undef LV_DEF_REFR_PERIOD
#endif
#define LV_DEF_REFR_PERIOD 16

/* Better handling of scattered updates (multiple small dirty areas). */
#ifdef LV_INV_BUF_SIZE
#undef LV_INV_BUF_SIZE
#endif
#define LV_INV_BUF_SIZE 32

/*==================
 * Display drivers
 *==================*/

/* Enable LVGL's upstream ST7789 driver (generic MIPI DBI/DCS). */
#ifdef LV_USE_ST7789
#undef LV_USE_ST7789
#endif
#define LV_USE_ST7789 1

/* LV_USE_GENERIC_MIPI is derived from per-controller flags in the HPM SDK config,
 * but since this file is included afterwards we must override it explicitly. */
#ifdef LV_USE_GENERIC_MIPI
#undef LV_USE_GENERIC_MIPI
#endif
#define LV_USE_GENERIC_MIPI 1

#endif /* HPM_LVGL_SPI_LV_CONF_EXT_H */

