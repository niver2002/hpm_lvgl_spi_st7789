# Copyright (c) 2026 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

# Enable LVGL with custom portable (we provide display + tick source)
set(CONFIG_LVGL 1)
set(CONFIG_LVGL_CUSTOM_PORTABLE 1)

# LVGL demos (menu launches selected demo)
set(CONFIG_LV_DEMO 1)

# Recommended (official) SPI DMA backend (HPM SDK components/spi + dma_mgr)
set(CONFIG_HPM_SPI 1)
set(CONFIG_DMA_MGR 1)

set(STACK_SIZE 0x8000)

# Default build type
if("${HPM_BUILD_TYPE}" STREQUAL "")
    set(HPM_BUILD_TYPE flash_xip)
endif()

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(lvgl_demos_menu)

# LVGL DEMOS (keep consistent with HPM SDK samples/lvgl)
sdk_compile_definitions(-DLV_USE_DEMO_MUSIC=1)
sdk_compile_definitions(-DLV_USE_DEMO_WIDGETS=1)
sdk_compile_definitions(-DLV_USE_DEMO_BENCHMARK=1)
sdk_compile_definitions(-DLV_USE_DEMO_RENDER=0)
sdk_compile_definitions(-DLV_USE_DEMO_STRESS=1)
sdk_compile_definitions(-DLV_USE_DEMO_FLEX_LAYOUT=1)

# LVGL driver config overrides for small SPI panels (ST7789 generic MIPI)
sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG="lv_conf_ext.h")

# LVGL SPI display component (this repo)
set(LVGL_SPI_DISPLAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

sdk_inc(${LVGL_SPI_DISPLAY_DIR})
sdk_src(
    ${LVGL_SPI_DISPLAY_DIR}/st7789.c
    ${LVGL_SPI_DISPLAY_DIR}/hpm_lvgl_spi.c
)

sdk_app_src(main.c)
sdk_app_inc(.)

generate_ide_projects()

