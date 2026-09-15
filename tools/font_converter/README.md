# Inter SemiBold → 2bpp 点阵字库转换器

把上游 **Inter SemiBold** 静态 TTF 栅格化成固件可直接使用的精简点阵字库。
固件运行时不解析 TTF、不做字模缩放。

对应实施计划：`doc/LCD_DISPLAY_IMPLEMENTATION_PLAN.md` §6。

## 1. 目录内容

| 文件 | 说明 |
|---|---|
| `generate_fonts.py` | 转换脚本（唯一入口，输出可重复） |
| `Inter-SemiBold.ttf` | 上游字体，来自 Inter 发行包 `extras/ttf/` |
| `LICENSE.txt` | 上游许可证（SIL Open Font License 1.1），随字体一起保留 |

字体来源：<https://github.com/rsms/inter>（发行包 `Inter-4.1.zip`）。
若需要重新获取：

```bash
curl -L -o Inter-4.1.zip https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
unzip -j Inter-4.1.zip extras/ttf/Inter-SemiBold.ttf LICENSE.txt -d tools/font_converter/
```

## 2. 重新生成

依赖：Python 3 + Pillow。

```bash
python tools/font_converter/generate_fonts.py
```

可选参数：`--ttf <路径>`、`--out-dir <目录>`。

输出位置：`Code/UserApp/ui/fonts/`。

## 3. 字符子集

| 字库 | 像素高度 | 字符子集 | 字形数 | 位图 | 字形表 |
|---|---:|---|---:|---:|---:|
| `font_inter_24` | 24 | `0123456789.-` | 12 | 698 B | 192 B |
| `font_inter_16` | 16 | `0123456789.-ONF` | 15 | 474 B | 240 B |
| `font_inter_8` | 8 | ` A-Z0-9.-/` | 40 | 504 B | 640 B |
| **合计** | | | 67 | **1676 B** | **1072 B** |

合计 2748 字节（全部为 `.rodata`，不占 SRAM）。

> 与实施计划 §6.2 的差异：16px 字库在数字之外额外带了 `O`/`N`/`F`，
> 因为 OUTPUT 区域的 `ON` / `OFF` 用的是 16px 字号。8px 字库包含了完整
> 大写字母表，便于后续加标签时不必重新生成字模。

**注意**：渲染器对字库里没有的字符是静默跳过。改界面文案时，
`tools/ui_preview/preview_main.c` 的 `TestGlyphCoverage()` 会做覆盖检查，
文案改完请跑一次预览（见 `tools/ui_preview/README.md`）。

## 4. 点阵格式

- 每像素 **2bpp**，取值 `0..3`：`0` 全透明、`3` 全前景、`1`/`2` 对应 1/3、2/3 覆盖；
- 每字节打包 4 个像素，**高位在前**（像素 n 位于 bit `6 - 2*(n % 4)`）；
- 每个字形独立存放，**每行按字节重新对齐**（不跨行拼接）；
- 位图按"所有字形按字符顺序拼接"存放，`UiGlyph.bitmap_offset` 给出每个字形的起始字节；
- 覆盖度是**线性量化**：`alpha = round(255 * v / 3)`，
  渲染端按 `(fg * a + bg * (3 - a)) / 3` 混合，因此这里不做伽马修正。

字形描述结构见 `Code/UserApp/ui/ui_font.h` 的 `UiGlyph`。

## 5. 数字前进宽度

Inter 默认数字是**比例宽度**（`0` 宽 16px、`1` 宽 10px @24px），
会让 `12.08` 与 `11.11` 的总宽不同、数值变化时左右跳动。
脚本在生成阶段把 `0..9` 的前进宽度统一为**最大数字宽度**，
并把字形墨迹在该宽度内**居中**（`x_offset = (advance - width) / 2`），
从而保证：

- `UiFont_MeasureText()` 对任何 `xx.xx` 都返回同一个宽度（24px：72px）；
- 小数点位置固定，数值刷新时不会抖动。

`tools/ui_preview` 的 `TestMetricStability()` 会对全部取值组合做断言。
