# HPM SDK `samples/lvgl` UI: make it usable on 172×320 (ST7789 class panels)

HPM SDK 的 `samples/lvgl` 默认偏向“较大分辨率”的屏（常见是 RGB/LCDC 屏）。在 172×320 这种窄屏上，默认的 demo 选择界面会出现：

- 5 个按钮横排太挤（文字挤压、触控目标太小）
- 部分示例（如 USB 键鼠 demo）控件宽度写死，超出屏幕

本仓库提供了一个可直接运行在 172×320 上的 demo 菜单示例：`examples/lvgl_demos_menu`，
同时也提供了一个给 HPM SDK 直接打补丁的 patch 文件。

## 方案 A：直接用本仓库的示例（推荐）

构建 `examples/lvgl_demos_menu`（与本仓库其它 example 用法一致），它会：

- 使用本仓库的 `hpm_lvgl_spi_init()`（SPI + DMA）
- 提供适配窄屏的 demo 菜单布局（窄屏自动单列大按钮）

## 方案 B：给 HPM SDK 打补丁（只改 UI，不动底层）

本仓库的补丁文件：`patches/hpm_sdk_lvgl_ui_small_screen.patch`

它会修改 HPM SDK 的两个文件：

- `samples/lvgl/common/lvgl.c`：把 5 列 grid 改成响应式 flex（窄屏单列）
- `samples/lvgl/lvgl_indev_usb_keyboard_mouse/src/main.c`：控件宽度改为百分比，标题自动换行

在你的 `hpm_sdk` 根目录执行：

```bash
git apply <path-to-this-repo>/patches/hpm_sdk_lvgl_ui_small_screen.patch
```

如果 HPM SDK 版本更新导致冲突：

- 先用 `git apply --reject --whitespace=fix ...` 让它生成 `.rej`
- 再手工把思路迁移过去（核心就是：不要写死宽度，窄屏用单列/滚动）

## 重要说明：这里的 “MIPI” 指什么？

LVGL 的 `lv_st7789`/`lv_lcd_generic_mipi` 这里说的 “MIPI” 是 **MIPI DBI/DCS 指令集**（很多 SPI 屏控制器复用这套命令），
不是 “MIPI DSI” 那种高速差分物理层接口。

所以：即使 HPM6E 没有 MIPI DSI 外设，依然可以通过 **SPI（配合 DMA）** 正常驱动 ST7789。

