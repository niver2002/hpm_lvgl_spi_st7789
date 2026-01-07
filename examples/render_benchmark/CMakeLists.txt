# Copyright (c) 2024 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

set(APP_VERSION_STRING "\"1.11.0\"")

# Enable LVGL with custom porting
set(CONFIG_LVGL 1)
set(CONFIG_LVGL_CUSTOM_PORTABLE 1)

# Recommended (official) SPI DMA backend (HPM SDK components/spi + dma_mgr)
set(CONFIG_HPM_SPI 1)
set(CONFIG_DMA_MGR 1)
set(STACK_SIZE 0x8000)

# Default build type
if("${HPM_BUILD_TYPE}" STREQUAL "")
    set(HPM_BUILD_TYPE flash_xip)
endif()

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(render_benchmark_demo)

# LVGL SPI display component (this repo)
set(LVGL_SPI_DISPLAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

sdk_compile_definitions(-DBOARD_SHOW_CLOCK=1)
sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG="lv_conf_ext.h")

sdk_inc(${LVGL_SPI_DISPLAY_DIR})
sdk_src(
    ${LVGL_SPI_DISPLAY_DIR}/st7789.c
    ${LVGL_SPI_DISPLAY_DIR}/hpm_lvgl_spi.c
)

sdk_app_src(main.c)
sdk_app_inc(.)

generate_ide_projects()
