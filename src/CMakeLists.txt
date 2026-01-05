# Copyright (c) 2024 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

sdk_inc(.)

sdk_src(
    st7789.c
    hpm_lvgl_spi.c
)

# Link LVGL middleware
sdk_compile_definitions(-DLV_CONF_INCLUDE_SIMPLE)
