# 原理图资料目录（pd-spoofing V0.1）

源文件：`../SCH_Schematic1_2026-09-14.pdf`（A3，1 页，矢量，嘉立创EDA 导出，2026-09-14）

本目录把该 PDF 的原理图信息**落盘为可检索、可 diff 的文本**，便于固件开发、BOM 整理和改板评审。

## 快速上手

- 想了解**整板功能与设计意图** → 读 [`SCHEMATIC_DESIGN.md`](SCHEMATIC_DESIGN.md)
- 想查**某个网络接在哪里 / 某个引脚干什么** → 读 [`NETLIST.md`](NETLIST.md)
- 想**采购或核对 BOM** → 读 [`COMPONENTS.md`](COMPONENTS.md)
- 想**看图上某处在哪 / 自己再挖细节** → 用 [`EXTRACTION_NOTES.md`](EXTRACTION_NOTES.md) 里的命令 + `02_textlayer_coords.tsv`

## 文件清单

| 文件 | 内容 |
|---|---|
| `SCHEMATIC_DESIGN.md` | 设计说明：图纸信息、系统框图、功率路径、MCU 逐脚表、外设、网络清单、读图存疑项 |
| `NETLIST.md` | 网络连接表：逐网络端点明细 + 关键器件引脚↔网络 + 电源通路速查 |
| `COMPONENTS.md` | 元件清单：有源器件、二极管待补项、连接器、电阻/电容全表、采购待办 |
| `EXTRACTION_NOTES.md` | 提取方法与可复现命令、各项信息可靠度评估 |
| `01_textlayer_raw.txt` | PDF 矢量文字层原文（阅读顺序，UTF-8） |
| `02_textlayer_coords.tsv` | 文字层坐标表 `x_pt / y_pt / size / text` |
| `crops/00_full_page.png` | 整页预览（200 dpi 等效） |
| `crops/01_usb_pd_input.png` | USB-C 座、CC 下拉、ESD |
| `crops/02_3v3_boost.png` | 3V3 DCDC 及其周边 |
| `crops/03_current_sense_out.png` | INA180 采样 + Q1/Q2 输出开关 + XT30 |
| `crops/04_adc_dividers.png` | VOUT / VBUS 分压采样 |
| `crops/05_mcu.png` | MCU 全貌（引脚↔网络） |
| `crops/06_keys_leds_buzzer.png` | 按键、蜂鸣器 |
| `crops/07_mcu_vdd_decoupling.png` | MCU VDD 去耦与 CC/VBUS-ADC 引脚特写 |
| `crops/08_dcdc_detail.png` | L1 / D1 / C4 / C5 / R5 / R6 特写（存疑拓扑） |
| `crops/09_ws2812_c23.png` | WS2812C-V6 链与去耦特写 |

## 关键结论速览

| 项 | 值 |
|---|---|
| 主控 | CH32X035G8U6（QFN-28） |
| 输入 | USB-C，CC1/CC2 各 5.1kΩ 下拉（受电），CC 同时接 MCU PC14/PC15 做 PD 协商 |
| 3V3 | DCDC1：L1 10µH + D1，FB = R5 124kΩ / R6 40.2kΩ，BST = C1 100nF |
| 电流采样 | R7 10mΩ + INA180A2（50V/V）→ IBUS-ADC，满量程 ≈ 6.6A |
| 电压采样 | VOUT、USB-VBUS 各 470k/68k 分压（≈1:7.9，满量程 ≈26V） |
| 输出开关 | Q1+Q2 G200P04D3 背靠背 + D2 10V 钳位，MCU PB12 (VOUT-EN) 经 Q3 驱动 |
| 输出端子 | CN1 = XT30PW-M30.G.Y |
| 显示 | LCD1 N114-2413THBIG01-H13（SPI + CS/DC/RES，背光常亮） |
| 交互 | 3 键（低有效，10k 上拉 + 10nF 消抖）、4× WS2812C-V6、2.7kHz 蜂鸣器、1 路 LED |
| 调试 | H1 = SWD（PC19/PC18 经 100Ω），H2 = UART（PB10/PB11 经 100Ω） |
| 图纸 TODO | ① BOOT ② GND 孔 ③ VBUS 背面散热 ④ LCD 丝印遮挡焊盘 |

## 打样前必须确认的三件事

1. **DCDC1 拓扑**：图中 L1 在 SW→3V3、D1 从 SW 对地，需对照所选 DCDC 规格书确认（详见 `SCHEMATIC_DESIGN.md` §8.1）。
2. **BOOT 电路**：图纸明确标为 TODO，PC18/DIO 目前只到 H1。
3. **D1、D3~D6、DCDC1 的型号**：图纸未标，BOM 必须补（详见 `COMPONENTS.md` §7）。
