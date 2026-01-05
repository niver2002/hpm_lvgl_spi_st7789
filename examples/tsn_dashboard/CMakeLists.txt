# Copyright (c) 2024 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

set(APP_VERSION_STRING "\"1.11.0\"")

# HPM_APPS路径 - 必须在find_package之前设置
set(HPM_APPS_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# 自定义板级搜索路径 - 指向hpm_apps/boards
set(BOARD_SEARCH_PATH ${HPM_APPS_BASE}/boards)

# 启用LVGL - 使用自定义porting
set(CONFIG_LVGL 1)
set(CONFIG_LVGL_CUSTOM_PORTABLE 1)
set(STACK_SIZE 0x8000)

# 默认构建类型
if("${HPM_BUILD_TYPE}" STREQUAL "")
    SET(HPM_BUILD_TYPE flash_xip)
endif()

# 使用HPM SDK的cmake框架
find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(tsn_dashboard_demo)

# LVGL SPI显示组件路径
set(LVGL_SPI_DISPLAY_DIR ${HPM_APPS_BASE}/boards/hpm6e00_full_port/components/lvgl_spi_display)

# SDK配置
sdk_compile_definitions(-DBOARD_SHOW_CLOCK=1)

# LVGL额外配置 - 指定自定义lv_conf.h
sdk_compile_definitions(-DCONFIG_LV_HAS_EXTRA_CONFIG="lv_conf.h")

# 添加板级组件路径（lv_conf.h所在位置）
sdk_inc(${LVGL_SPI_DISPLAY_DIR})

# 添加板级LVGL SPI显示组件源文件
sdk_src(
    ${LVGL_SPI_DISPLAY_DIR}/st7789.c
    ${LVGL_SPI_DISPLAY_DIR}/hpm_lvgl_spi.c
)

# 应用源文件
sdk_app_src(src/main.c)
sdk_app_inc(src)

# 引入hpm_apps构建系统
add_subdirectory(../.. hpm_apps/build_tmp)

# 生成IDE工程
generate_ide_projects()
