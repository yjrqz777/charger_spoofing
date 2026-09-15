# ui_preview — 界面 PC 端预览与断言

在没有硬件的情况下验证 `Code/UserApp/ui/` 的界面逻辑：
数值格式化、字距稳定性、基线/布局边界、颜色、按需刷新量。

它把 `Code/UserApp/ui/ui_font.c` 和 `ui_dashboard.c` **原样**编译到 PC 上，
只把 LCD 底层换成内存帧缓冲（`lcd_sim.c`），因此验证的是真实的固件代码路径，
而不是另写一份 Python 复刻版。

## 1. 依赖

- MinGW gcc（或任意能编译 gnu99 的 C 编译器）
- Python 3 + Pillow（只用于把 `out/*.ppm` 转成 PNG 方便查看）

## 2. 编译与运行

```bash
cd tools/ui_preview
gcc -std=gnu99 -Wall -Wextra -I stub -I ../../Code/UserApp/ui \
    preview_main.c lcd_sim.c \
    ../../Code/UserApp/ui/ui_font.c \
    ../../Code/UserApp/ui/ui_dashboard.c \
    ../../Code/UserApp/ui/fonts/font_inter_8.c \
    ../../Code/UserApp/ui/fonts/font_inter_16.c \
    ../../Code/UserApp/ui/fonts/font_inter_24.c \
    -o ui_preview.exe
./ui_preview.exe
```

`-I stub` 必须排在前面：它用 `stub/main.h` 和 `stub/st7789v/st7789v.h`
顶掉固件的板级头文件。

退出码为 0 表示全部断言通过。

## 3. 断言内容

| 编号 | 内容 |
|---:|---|
| 1 | 数值宽度一致性：所有 `xx.xx` / `x.xx` / `xx.x` 取值宽度恒定 |
| 2 | 字库覆盖：界面用到的每个字符串都能在对应字库里找到字形 |
| 3 | 布局约束：每个区域内除设计矩形外，其余像素必须仍是底色（不允许内容越界） |
| 4 | 结构检查：分隔线整条 1px、卡片四角为边框色、关键颜色确实出现 |
| 5 | 稳态刷新量：只改一路数据 / 只翻开关时的矩形数与像素数 |

## 4. 输出

`out/` 下每个场景一份 PPM（原始 240×135）与 PNG（3 倍放大便于查看）：

| 文件 | 场景 |
|---|---|
| `01_running_on` | 20.08 V / 19.96 V / 1.22 A / 24.5 W，VOUT-EN = ON |
| `02_running_off` | 输出关断（`OFF`，灰色滑块在左） |
| `03_invalid` | 采样无效（`--.--`、`--.-`，顶部状态栏留空） |
| `04_max` | 各字段饱和值，检查位数不会越出设计矩形 |

`out/report.txt` 是最近一次的运行日志。

## 5. 局限

- 只验证界面层，不验证 ST7789V 的窗口/字节序/SPI 时序；
  底层 `LCD_FillRect()` / `LCD_BlitRect()` 仍需要上板确认。
- `lcd_sim.c` 把大端字节序的 RGB565 还原成像素，与固件
  `st7789v.c` 中 `BSP_Lcd` 的写字节顺序一致，但 DMA 时序、`LCD_Address_Set()`
  的列/行偏移（`USE_HORIZONTAL`）不在验证范围内。
