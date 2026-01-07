/**
 * @file lv_conf.h
 * Configuration file for LVGL v9.x
 * HPM6E00 FULL_PORT - ST7789/GC9307 SPI Display
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16 (RGB565) for ST7789 */
#define LV_COLOR_DEPTH 16

/*=========================
   MEMORY SETTINGS
 *=========================*/

/* Size of the memory available for `lv_malloc()` in bytes (>= 2kB) */
#define LV_MEM_SIZE (48 * 1024U)

/*=======================
   DISPLAY SETTINGS
 *=======================*/

/* Default display refresh, input device read and animation step period. */
#define LV_DEF_REFR_PERIOD  16      /* 16ms = ~60Hz refresh */

/* Default DPI. */
#define LV_DPI_DEF 130

/*=========================
   DIRTY AREA OPTIMIZATION
 *=========================*/

/* Maximum number of invalid areas saved for refresh.
 * Larger = better handling of scattered updates (multiple buttons, etc.)
 * Each area is a rectangle that needs to be refreshed */
#define LV_INV_BUF_SIZE 32

/* Draw buffer row-by-row rendering
 * 0 = Render the whole area at once (uses more memory but more efficient)
 * 1 = Render row-by-row (saves memory but slower for small areas) */
#define LV_DRAW_BUF_STRIDE_ALIGN 1

/* Enable area merging to reduce flush calls
 * LVGL will combine overlapping/adjacent dirty rectangles */
#define LV_REFR_MERGE_AREAS 1

/*=================
   FONT USAGE
 =================*/

/* Montserrat fonts with ASCII range and some symbols */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1

/* Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*===================
   WIDGET USAGE
 *==================*/

/* Documentation of the widgets: https://docs.lvgl.io/latest/en/html/widgets/index.html */

#define LV_USE_ANIMIMG    1
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BUTTON     1
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     0
#define LV_USE_CHART      1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   0
#define LV_USE_IMAGE      1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD   0
#define LV_USE_LABEL      1
#define LV_USE_LED        1
#define LV_USE_LINE       1
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SCALE      1
#define LV_USE_SLIDER     1
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   1
#define LV_USE_WIN        0

/*==================
 * LAYOUTS
 *==================*/

/* A simple row-column layout */
#define LV_USE_FLEX 1

/* A grid-like layout */
#define LV_USE_GRID 1

/*==================
 * THEMES
 *==================*/

/* Simple theme with no extra files */
#define LV_USE_THEME_DEFAULT 1

#if LV_USE_THEME_DEFAULT
    /* 0: Light mode; 1: Dark mode */
    #define LV_THEME_DEFAULT_DARK 1

    /* 1: Enable grow on press */
    #define LV_THEME_DEFAULT_GROW 0

    /* Default transition time in [ms] */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

/* Simple theme without styles */
#define LV_USE_THEME_SIMPLE 0

/*==================
 * OTHERS
 *==================*/

/* Enable the log module */
#define LV_USE_LOG 0

/* Enable asserts */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* Enable performance monitor */
#define LV_USE_PERF_MONITOR 0

/* Enable memory monitor */
#define LV_USE_MEM_MONITOR 0

/* 1: Use custom tick source that tells the elapsed time in milliseconds.
 * It removes the need to manually update the tick with `lv_tick_inc()`) */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    extern uint32_t custom_tick_get(void);
    #define LV_TICK_CUSTOM_INCLUDE <stdint.h>
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (custom_tick_get())
#endif

/* Enable file system */
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0

/*==================
 * DISPLAY DRIVERS
 *==================*/

/* Enable LVGL's built-in ST7789 driver (Generic MIPI DBI/DCS).
 * This matches the HPM SDK LVGL driver layout under:
 * `middleware/lvgl/lvgl/src/drivers/display/st7789` */
#ifdef LV_USE_ST7789
#undef LV_USE_ST7789
#endif
#define LV_USE_ST7789 1

/* Generic MIPI driver is required by LVGL ST7789 wrapper. */
#ifdef LV_USE_GENERIC_MIPI
#undef LV_USE_GENERIC_MIPI
#endif
#define LV_USE_GENERIC_MIPI 1

/*===================
 * EXAMPLES
 *==================*/

/* Enable built-in examples */
#define LV_BUILD_EXAMPLES 0

/*===================
 * DEMOS
 *==================*/

/* Disable all demos */
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS 0
#define LV_USE_DEMO_MUSIC 0

#endif /*LV_CONF_H*/
