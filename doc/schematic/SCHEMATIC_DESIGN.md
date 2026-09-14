# pd-spoofing 原理图设计说明（V0.1）

> 本文档由原理图 `doc/SCH_Schematic1_2026-09-14.pdf` 的矢量文字层 + 高倍渲染图像区读图整理而成。
> 所有网络名、元件标号、参数均取自图纸原文字层，未做推测补全；存在歧义处集中列在 §8「读图存疑/待确认」。

## 0. 图纸信息

| 项 | 内容 |
|---|---|
| 源文件 | `doc/SCH_Schematic1_2026-09-14.pdf` |
| 页数 / 幅面 | 1 页 / A3（1786.25 x 861.48 pt） |
| 图纸名 | `Schematic1` |
| 板名 | `pd-spoofing` |
| 图页名 | `charger_spoofing` |
| 版本 | V0.1 |
| 创建 / 更新 | 2026-09-09 / 2026-09-14 |
| 绘制 / 审阅 | YJRQZ / （空） |
| EDA 工具 | 嘉立创EDA |
| 图形类型 | 矢量（1427 个绘图图元，无位图），文字层 568 个 span |

### 图纸自带 TODO（原文照抄）

1. `1: BOOT`
2. `2: GND孔`
3. `3: VBUS背面散热`
4. `4: LCD丝印遮挡到焊盘`

### 一句话功能定位

从 USB-C 充电器取电（CC 上挂 5.1kΩ 做受电握手），把 VBUS 升压/变换成 3V3 给板载 MCU 与 LCD 供电；
同时用 MCU（CH32X035G8U6）挂在 CC1/CC2 上参与 PD 协商，并对输出做电流采样、电压采样与通断控制，
输出经 XT30 端子 `CN1` 对外供电；板载 LCD、4 颗 WS2812、蜂鸣器、3 个按键与 2 个排针做交互与调试。

---

## 1. 系统框图（按图纸信号流）

```
USB-C (USB1)                DCDC1 (3V3)              MCU U2 CH32X035G8U6
  A4B9/B4A9 VBUS ──┬──► IN ── SW/L1/D1 ──► 3V3 ──┬──► VDD(2) + C13 10uF/C14 1uF
  A1B12/B1A12 GND  │   C2 4.7uF + C3 100nF      ├──► LCD1 VCC/LEDA (经 R2 100Ω, C6 100nF)
  A5 CC1 ─ R4 5.1kΩ┼─► GND                      ├──► U3~U6 WS2812 + C19~C22 100nF
  B5 CC2 ─ R1 5.1kΩ┼─► GND                      ├──► H1/H2 排针 1 脚
  A6/A7/B6/B7 D± ──┼─► USB-DP/USB-DM          └──► R19/R20/R21 10kΩ 按键上拉
  A8 SBU1 / B8 SBU2└─► NC(打叉)
                   │
  CC1/CC2 ─────────┴──► MCU PC14(28) / PC15(1)   ← PD 协商走 CC

USB-VBUS ─► R7 10mΩ ─► Q1/Q2(G200P04D3 背靠背) ─► VOUT ─► CN1 (XT30PW-M30.G.Y)
              │                    ▲
              │                    └── D2 BZT52C10(10V) 钳位 / R8 100kΩ G-S 上拉 / R15 10kΩ 栅极
              │                         └── Q3 2N7002 ◄─ R17 1kΩ ◄─ VOUT-EN(MCU PB12)
              └── U1 INA180A2IDBVR (增益 50V/V) ─► R16 100Ω ─► IBUS-ADC (MCU PA4)
```

---

## 2. 功率路径

### 2.1 Type-C 接口 `USB1`（TYPE-C 16PIN 2MD(073)，16P 贴板母座）

| 焊盘 | 网络 | 说明 |
|---|---|---|
| A5 | USB-CC1 | 经 R4 5.1kΩ 下拉到 GND；同时进 MCU PC14 |
| B5 | USB-CC2 | 经 R1 5.1kΩ 下拉到 GND；同时进 MCU PC15 |
| A6 / B6 | USB-DP | Dp1 A6 与 Dp2 B6 短接 |
| A7 / B7 | USB-DM | Dn1 A7 与 Dn2 B7 短接 |
| A4B9 / B4A9 | USB-VBUS | 两排 VBUS 合并 |
| A1B12 / B1A12 | GND | |
| A8 (SBU1) / B8 (SBU2) | NC | 图纸打叉悬空 |
| 13 / 14 (EH) | GND | 两位屏蔽/固定脚接地 |

- **CC 下拉 5.1kΩ + ESD `D3~D6`**：`D3→USB-DP`、`D4→USB-DM`、`D5→USB-CC1`、`D6→USB-CC2`，另一端均接 GND（D3~D6 图纸未标型号）。

### 2.2 3V3 电源 `DCDC1`

| 位号 | 连接 | 参数 |
|---|---|---|
| DCDC1 IN (5) | USB-VBUS | 输入，C2 4.7µF + C3 100nF 去耦到 GND |
| DCDC1 EN (4) | DCDC_EN | 由 R3 510kΩ 上拉到 USB-VBUS；SW3 可拉低关闭 |
| DCDC1 BST (1) | 经 C1 100nF 到 SW | 自举电容 |
| DCDC1 SW (6) | L1 10µH 与 D1 阳极 | 开关节点 |
| DCDC1 FB (3) | R5 124kΩ 到 3V3；R6 40.2kΩ 到 GND；C7 47pF 跨 FB-3V3（前馈） | 反馈网络 |
| DCDC1 GND (2) | GND | |
| 输出 | 3V3 | L1 10µH 串到输出；C4 22µF + C5 100nF 到 GND |
| D1 | SW 节点 ↔ GND | 图纸未标型号（体二极管符号朝上，即阴极接 SW 节点） |

### 2.3 电流采样 `U1 = INA180A2IDBVR`（A2 = 增益 50 V/V）

- 采样电阻 **R7 = 10mΩ** 串在 `USB-VBUS`（INA180 IN+ 侧）与 **Q1 源极**（IN− 侧）之间。
- 供电：VS (5) 接 3V3，C12 100nF 去耦；GND (2) 接 GND。
- 输出 OUT (1) → **R16 100Ω** → `IBUS-ADC` → MCU PA4(9)，**C11 10nF** 对地做 RC 滤波。
- 量程换算：满量程电流 = 3.3V ÷ 50 ÷ 10mΩ ≈ **6.6 A**（ADC 满量程 3.3V 时）。

### 2.4 输出通断开关

| 器件 | 连接 |
|---|---|
| Q1 / Q2 = G200P04D3（P-MOS，8 脚：5~8=D，1~3=S，4=G） | 两管源极对接成背靠背双向开关：Q1 漏极接 R7 后的 USB-VBUS 侧，Q2 漏极接 VOUT |
| D2 = BZT52C10（10V 稳压管） | 跨接 G-S 做栅极钳位 |
| R8 = 100kΩ | G-S 上拉（默认关断） |
| R15 = 10kΩ + R17 = 1kΩ | 栅极驱动串阻，下端由 Q3 拉低导通 |
| Q3 = 2N7002,215 | 栅极由 `VOUT-EN`（MCU PB12）经 R17 1kΩ 驱动，另有 100kΩ 栅极下拉（图纸 R14/R18 标号重叠在同一电阻上） |
| CN1 = XT30PW-M30.G.Y | 输出端子，VOUT / GND，含 4 个安装孔 |

### 2.5 电压采样分压（两路结构相同，分压比 470k : 68k ≈ 1 : 7.91，满量程约 26.1V）

| 通道 | 分压 | 串阻 | 滤波 | MCU 脚 |
|---|---|---|---|---|
| VOUT | R9 470kΩ / R13 68kΩ | R11 100Ω → `VOUT-ADC` | C9 100nF | PA0(5) |
| USB-VBUS | R10 470kΩ / R14 68kΩ | R12 100Ω → `USB-VBUS-ADC` | C10 100nF | PC0(3) |

> 另注：`C15` 为 `USB-VBUS-ADC` 节点附近的电容（画在 MCU 左侧 VDD/PC0 区域），归属 ADC 输入网络而非 MCU 电源去耦。

---

## 3. MCU `U2 = CH32X035G8U6` 完整引脚网络表

主控为 CH32X035G8U6（QFN-28 + 裸焊盘 29 = GND）。资料上另有 `USB1` 引出的 `USB-DP/USB-DM` 只到 ESD 与座子，**未接 MCU**。

| Pin | 引脚复用名（图纸原文） | 网络 | 备注 |
|---:|---|---|---|
| 1 | PC15/CC2/T2C3_/T1ET_ | USB-CC2 | PD 协商用 CC2（脚旁红点 = 1 脚标记） |
| 2 | VDD | 3V3 | C13 10µF + C14 1µF 去耦 |
| 3 | PC0/TX2_/T2C4_/T1C1_/T2BK_/A10 | USB-VBUS-ADC | VBUS 电压采样 |
| 4 | PC3/T1C4_/T2C3N_/T2C1N_/C1N0/A13 | —（悬空） | |
| 5 | PA0/T2C1/CTS2/C1P1/A0 | VOUT-ADC | 输出电压采样 |
| 6 | PA1/RTS2/T2C2/C1O/O1N2/O2N2/A1 | LCD_RES | LCD 复位 |
| 7 | PA2/TX2/T2C3/O2O1/T2ET_/A2 | LCD_DC | LCD 数据/命令 |
| 8 | PA3/T2C4/O1O0/T3C1_/RX2/A3 | LCD_CS | LCD 片选 |
| 9 | PA4/CS/O2O0/T3C2_/A4 | IBUS-ADC | 电流采样 |
| 10 | PA5/SCK/TX4_/O2N0/A5 | LCD_SCK | SPI 时钟 |
| 11 | PA6/MISO/T3C1/T1BK_/O1N0/A6 | —（悬空） | |
| 12 | PA7/MOSI/T3C2/T1C1N_/TX1_/O2P0/A7 | LCD_SDA | SPI 数据 |
| 13 | PB0/TX4/T1C2N_/O1P0/A8 | —（悬空） | |
| 14 | PB3/TX3/T2C3_/T2C3N_/O2P1 | KEY-1 | 按键 1（低有效） |
| 15 | PB4/T2C4_/T3C1_/T2BK_/RX3/O1P2 | —（悬空） | |
| 16 | PB5/O1O1/T3C2_/T1BK | —（悬空） | |
| 17 | PB6/T1C1N/CTS3/O1N1 | KEY-3 | 按键 3 |
| 18 | PB7/T1C2N/RTS3_/O2P2 | KEY-2 | 按键 2 |
| 19 | PB8/T1C3N/O1P1 | LED | 板载指示灯（经 LED1 + R37 100Ω 到 GND） |
| 20 | PB9/T1C1/MCO/TX4_ | WS2812 | 4 颗灯珠数据 |
| 21 | PB10/TX1/T1C2 | LOG-TX | 经 R34 100Ω 到 H2 |
| 22 | PB11/T1C3/T2C1N_/RX1 | LOG-RX | 经 R35 100Ω 到 H2 |
| 23 | PB12/T1C4_/T2C2N_ | VOUT-EN | 输出开关使能 |
| 24 | PC19/DCK/T2C1_/T3C1_/I2C_/RX3_/C1P0 | MCU-DCK | 经 R27 100Ω 到 H1（SWD 时钟） |
| 25 | PC18/DIO/TX3_/T2C1N_/T3C2_/I2C_/T1ET_ | MCU-DIO | 经 R28 100Ω 到 H1（SWD 数据） |
| 26 | PC16/UDM/T1C4/TX4_/I2C_/RX4_/CTS1/PC11 | —（悬空） | |
| 27 | PC17/UDP/RTS1/TX4_/I2C_/RX4_/T1ET/PC10 | —（悬空） | |
| 28 | PC14/CC1/T1C3_/T2C2_ | USB-CC1 | PD 协商用 CC1 |
| 29 | GND（裸焊盘） | GND | |

---

## 4. 显示部分 `LCD1 = N114-2413THBIG01-H13`

| Pin | 名称 | 连接 |
|---:|---|---|
| 1 | NC | 打叉未接 |
| 2 | NC | 打叉未接 |
| 3 | SDA | LCD_SDA（MCU PA7/12） |
| 4 | SCL | LCD_SCK（MCU PA5/10） |
| 5 | RS | LCD_DC（MCU PA2/7） |
| 6 | RES | LCD_RES（MCU PA1/6） |
| 7 | CS | LCD_CS（MCU PA3/8） |
| 8 | GND | GND |
| 9 | NC | 打叉未接 |
| 10 | VCC | 3V3 经 R2 100Ω 限流，C6 100nF 去耦 |
| 11 | LEDK | 直接接 GND |
| 12 | LEDA | 接在 R2 之后的 3V3 节点 |
| 13 | GND | GND |

要点：**背光 LEDK 直接接地 → 背光常亮，图纸上没有做背光开关/PWM 调光**。

---

## 5. 人机交互与外设

### 5.1 WS2812 灯链（4 颗，`WS2812C-V6`）

```
MCU PB9 ── WS2812 ── R26 100Ω ──▶ U6 DIN
                                   U6 DOUT ──▶ U5 DIN
                                                U5 DOUT ──▶ U4 DIN
                                                             U4 DOUT ──▶ U3 DIN
                                                                          U3 DOUT ── 悬空
```

每颗：VDD(1) = 3V3，VSS(3) = GND，DIN(4) = 上一级，DOUT(2) = 下一级。
去耦：C19/C20/C21/C22 = 100nF（每颗一颗）；另有 C23 = 22µF 在 3V3 上做灯链储能。

### 5.2 按键（3 路，结构相同，低电平有效）

| 位号 | 网络 | 结构 |
|---|---|---|
| SW1 = TS24CA | KEY-1 | 上拉 R19 10kΩ 到 3V3；C16 10nF 到 GND 硬件消抖；按下接 GND（4/1 脚） |
| SW2 = TS24CA | KEY-2 | 上拉 R20 10kΩ；C17 10nF |
| SW3 = TS24CA | KEY-3 | 上拉 R21 10kΩ；C18 10nF |

> SW1/SW2/SW3 的 3/4 脚接 GND，1/2 脚分别接 KEY 网络与 GND（4 脚轻触开关的对称接法）。

### 5.3 蜂鸣器（`BUZZER1 = 2.7kHz`）

`BEEP`（MCU 侧）→ R24 1kΩ → Q4（S8050，NPN）基极；R25 1kΩ 基极下拉到 GND；
Q4 发射极接 GND，集电极经 R23 100Ω → BUZZER1；蜂鸣器另一端接 3V3，另有 R22 1kΩ 上拉。

### 5.4 指示灯

`LED`（MCU PB8）→ LED1（LTST-C191KSKT）→ R37 100Ω → GND，高电平点亮。

### 5.5 调试/日志排针

| 排针 | 位号 | 型号 | 连接 |
|---|---|---|---|
| H1 | H1 | LAIL-PZ2.54-4P-L | 1=3V3，2=MCU-DCK（R27 100Ω），3=MCU-DIO（R28 100Ω），4=GND |
| H2 | H2 | LAIL-PZ2.54-4P-L | 1=3V3，2=LOG-TX（R34 100Ω），3=LOG-RX（R35 100Ω），4=GND |

> 按 2/3 脚串 100Ω、1/4 为电源地判断，H1 为 SWD 下载口（DCK/DIO），H2 为串口日志口（TX/RX）。

---

## 6. 全部网络标号清单（图纸原文字层）

| 网络 | 出现次数 | 归属 |
|---|---:|---|
| 3V3 | 15 | DCDC1 输出，全板数字/模拟供电 |
| USB-VBUS | 5 | Type-C VBUS |
| USB-DP / USB-DM | 4 / 4 | Type-C 数据线（仅座子+ESD） |
| USB-CC1 / USB-CC2 | 3 / 3 | CC 线（座子、ESD、MCU） |
| GND | 多处 | 地 |
| VOUT | 2 | 输出开关后电压 |
| VOUT-ADC | 2 | VOUT 分压采样 |
| USB-VBUS-ADC | 2 | VBUS 分压采样 |
| IBUS-ADC | 2 | R7 电流采样放大后 |
| VOUT-EN | 2 | 输出开关使能（MCU PB12） |
| DCDC_EN | 2 | 3V3 DCDC 使能（R3 上拉 + SW3） |
| LCD_CS / LCD_RES / LCD_DC / LCD_SCK / LCD_SDA | 2 各 | LCD SPI 与控制 |
| KEY-1 / KEY-2 / KEY-3 | 2 各 | 按键 |
| LED | 2 | 指示灯 |
| WS2812 | 2 | 灯链数据 |
| BEEP | 1 | 蜂鸣器驱动 |
| LOG-TX / LOG-RX | 2 各 | 日志串口到 H2 |
| MCU-DCK / MCU-DIO | 2 各 | SWD 到 H1 |

## 7. 元件清单（按图纸）

### 7.1 有源器件

| 位号 | 型号/参数 | 作用 |
|---|---|---|
| U1 | INA180A2IDBVR | 电流采样放大器，增益 50V/V |
| U2 | CH32X035G8U6 | 主控 MCU（QFN-28） |
| U3~U6 | WS2812C-V6 | 4 颗 RGB 灯珠 |
| Q1, Q2 | G200P04D3 | P-MOS，输出背靠背开关 |
| Q3 | 2N7002,215 | N-MOS，栅极驱动 |
| Q4 | S8050 | NPN，蜂鸣器驱动 |
| DCDC1 | 6 脚 DCDC（BST/GND/FB/EN/IN/SW） | 3V3 电源 |
| D1 | 未标注 | SW 节点续流二极管 |
| D2 | BZT52C10 | 10V 稳压管，G-S 钳位 |
| D3~D6 | 未标注 | DP/DM/CC1/CC2 ESD |
| LED1 | LTST-C191KSKT | 状态指示 LED |

### 7.2 连接器 / 机电

| 位号 | 型号 | 说明 |
|---|---|---|
| USB1 | TYPE-C 16PIN 2MD(073) | Type-C 母座 |
| CN1 | XT30PW-M30.G.Y | 输出端子 |
| LCD1 | N114-2413THBIG01-H13 | LCD 模组（13P） |
| H1, H2 | LAIL-PZ2.54-4P-L | 2.54mm 4P 排针 |
| SW1~SW3 | TS24CA | 4 脚轻触开关 |
| BUZZER1 | 2.7kHz | 蜂鸣器 |

### 7.3 电阻

| 位号 | 值 | 位号 | 值 | 位号 | 值 |
|---|---|---|---|---|---|
| R1 | 5.1kΩ | R11 | 100Ω | R21 | 10kΩ |
| R2 | 100Ω | R12 | 100Ω | R22 | 1kΩ |
| R3 | 510kΩ | R13 | 68kΩ | R23 | 100Ω |
| R4 | 5.1kΩ | R14 | 68kΩ | R24 | 1kΩ |
| R5 | 124kΩ | R15 | 10kΩ | R25 | 1kΩ |
| R6 | 40.2kΩ | R16 | 100Ω | R26 | 100Ω |
| R7 | 10mΩ | R17 | 1kΩ | R27 | 100Ω |
| R8 | 100kΩ | R18 | 100kΩ | R28 | 100Ω |
| R9 | 470kΩ | R19 | 10kΩ | R34 | 100Ω |
| R10 | 470kΩ | R20 | 10kΩ | R35 | 100Ω |
| | | | | R37 | 100Ω |

> 编号 R29~R33、R36 未出现在本页（跳号）。`VOUT-EN` 开关处图上有两个标号压在同一个电阻上（R14 与 R18 位置重叠），实际应是 100kΩ 栅极下拉。

### 7.4 电容

| 位号 | 值 | 归属 |
|---|---|---|
| C1 | 100nF | DCDC1 BST 自举 |
| C2 / C3 | 4.7µF / 100nF | DCDC1 输入 |
| C4 / C5 | 22µF / 100nF | 3V3 输出 |
| C6 | 100nF | LCD VCC 去耦 |
| C7 | 47pF | DCDC1 FB 前馈 |
| C8 | 10nF | （DCDC1 周边） |
| C9 / C10 | 100nF | VOUT / VBUS 分压滤波 |
| C11 | 10nF | IBUS-ADC 滤波 |
| C12 | 100nF | INA180 供电去耦 |
| C13 / C14 | 10µF / 1µF | MCU VDD 去耦 |
| C15 | 100nF | USB-VBUS-ADC 节点 |
| C16 / C17 / C18 | 10nF | 按键消抖 |
| C19~C22 | 100nF | 每颗 WS2812 去耦 |
| C23 | 22µF | 3V3 储能 |

---

## 8. 读图存疑 / 待确认（打样前建议核实）

1. **DCDC1 拓扑疑点（最需要确认）**：图中 L1 10µH 接在 SW 与 3V3 之间，D1 从 SW 节点接 GND，
   这是「电感在输出侧、二极管对地」的画法；而升压（boost）的标准画法是「电感在输入侧、二极管由 SW 指向输出」。
   怀疑 D1 实际应为 SW→3V3 的整流二极管，图纸绘制顺序与实际选型不一致，请对照 DCDC1 规格书确认。
2. **BOOT 未做**（图纸 TODO 1）：PC18/DIO 仅引到 H1，未见 BOOT 跳线/按键/下拉，需确认 CH32X035 的下载/进入方式。
3. **CC 上同时挂 5.1kΩ 下拉与 MCU 引脚**：作为受电设备 5.1kΩ 正确，但若 MCU 要在 CC 上做 PD（BMC）收发，
   图纸未画出任何耦合/隔离电路，需确认 MCU 能否直接挂 CC 作 PHY。
4. **背光常亮**：LEDK 直接接地，无调光能力（如为有意简化可忽略）。
5. **ESD 与 D1 无型号**：D1、D3~D6 图纸未标具体型号，BOM 需补。
6. **`VOUT-EN` 栅极下拉标号重叠**（R14/R18），实际数量与位号需与 PCB/BOM 对齐。
7. 图纸 TODO 2/3/4 属 PCB 层面（GND 孔、VBUS 背面散热、LCD 丝印压焊盘），本说明未覆盖。

---

## 9. 附：资料索引

| 文件 | 内容 |
|---|---|
| `SCHEMATIC_DESIGN.md` | 本文档（设计说明，人读） |
| `NETLIST.md` | 网络-引脚连接表（结构化，便于核对固件/PCB） |
| `COMPONENTS.md` | 元件清单（BOM 视角） |
| `01_textlayer_raw.txt` | PDF 矢量文字层原文（阅读顺序，UTF-8） |
| `02_textlayer_coords.tsv` | 文字层坐标表（x/y/字高/文本），可据此定位任何标号在图上的位置 |
| `EXTRACTION_NOTES.md` | 提取方法与可复现步骤（含重新生成的命令） |
| `crops/*.png` | 分区域高倍裁图 01~09 |
