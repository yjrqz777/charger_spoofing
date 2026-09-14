# pd-spoofing 网络连接表（Netlist）

> 来源：`doc/SCH_Schematic1_2026-09-14.pdf` 矢量文字层 + 分区高倍读图。
> 用途：核对固件引脚定义、PCB 网络、飞线测试点。板名 `pd-spoofing`，图页 `charger_spoofing`，版本 V0.1。

## 0. 网络总览（按功能分组）

| 分组 | 网络 |
|---|---|
| 电源 | `USB-VBUS`、`3V3`、`GND` |
| USB | `USB-DP`、`USB-DM`、`USB-CC1`、`USB-CC2` |
| 电源控制 | `DCDC_EN`、`VOUT`、`VOUT-EN` |
| 采样 | `VOUT-ADC`、`USB-VBUS-ADC`、`IBUS-ADC` |
| 显示 | `LCD_CS`、`LCD_RES`、`LCD_DC`、`LCD_SCK`、`LCD_SDA` |
| 交互 | `KEY-1`、`KEY-2`、`KEY-3`、`LED`、`WS2812`、`BEEP` |
| 调试 | `MCU-DCK`、`MCU-DIO`、`LOG-TX`、`LOG-RX` |

## 1. 逐网络连接明细

### USB-VBUS
| 端点 | 器件·引脚 |
|---|---|
| USB1 | VBUS A4B9 / B4A9 |
| C2 | 4.7µF → GND |
| C3 | 100nF → GND |
| R3 | 510kΩ（上拉 `DCDC_EN`） |
| DCDC1 | IN (5) |
| R7 | 10mΩ 端子 1（INA180 **IN+** 侧） |
| R10 | 470kΩ（`USB-VBUS-ADC` 分压上臂） |
| D6 | USB-CC2 无关；D3~D6 阳极侧网络为 DP/DM/CC1/CC2 |

### 3V3
| 端点 | 器件·引脚 |
|---|---|
| DCDC1 | 输出侧 L1 10µH 之后 |
| C4 / C5 | 22µF / 100nF → GND |
| C23 | 22µF → GND |
| R5 | 124kΩ（DCDC1 FB 反馈上臂） |
| C7 | 47pF（跨 FB–3V3 前馈） |
| U2 | VDD (2)；C13 10µF、C14 1µF 去耦 |
| R2 | 100Ω（LCD 供电限流）→ C6 100nF → LCD1 VCC(10) / LEDA(12) |
| U3~U6 | WS2812C-V6 VDD(1)，各配 C19~C22 100nF |
| U1 | INA180 VS (5)，C12 100nF 去耦 |
| R19 / R20 / R21 | 10kΩ 按键上拉到 3V3 |
| R22 | 1kΩ（蜂鸣器上拉） |
| BUZZER1 | 一端接 3V3 |
| H1 / H2 | 1 脚 |

### GND
USB1（A1B12/B1A12、13/14 EH）、R1/R4 5.1kΩ 下端、D3~D6 阴极、USB-DP/DM/CC1/CC2 的 ESD 侧、
C2/C3/C4/C5/C6/C9/C10/C11/C12/C13/C14/C15/C16/C17/C18/C19~C22/C23、D1 阴极、R6 40.2kΩ、R13/R14 68kΩ、
DCDC1 GND(2)、U1 GND(2)、U2 GND(29)、D2/R8（G-S 回路）、Q3 源极、R25 基极下拉、Q4 发射极、SW1~SW3（3/4 脚）、
LED1→R37 100Ω 下端、C20/C21 等、CN1 负端、H1/H2 4 脚。

### USB-DP
USB1 Dp1 A6 + Dp2 B6（短接）→ D3（ESD 到 GND）→ `USB-DP` → U2 PC17/UDP (27)。

### USB-DM
USB1 Dn1 A7 + Dn2 B7（短接）→ D4（ESD 到 GND）→ `USB-DM` → U2 PC16/UDM (26)。

### USB-CC1
| 端点 | 说明 |
|---|---|
| USB1 | CC1 A5 |
| R4 | 5.1kΩ → GND（受电握手电阻） |
| D5 | ESD → GND |
| U2 | PC14/CC1 (28) |

### USB-CC2
| 端点 | 说明 |
|---|---|
| USB1 | CC2 B5 |
| R1 | 5.1kΩ → GND |
| D6 | ESD → GND |
| U2 | PC15/CC2 (1) |

### DCDC_EN
| 端点 | 说明 |
|---|---|
| DCDC1 | EN (4) |
| R3 | 510kΩ 上拉到 USB-VBUS |
| SW3 | TS24CA，按下拉到 GND（关闭 3V3） |

### VOUT
| 端点 | 说明 |
|---|---|
| Q2 | 漏极（背靠背开关输出侧） |
| CN1 | XT30 正端 |
| R9 | 470kΩ（`VOUT-ADC` 分压上臂） |

### VOUT-EN
| 端点 | 说明 |
|---|---|
| U2 | PB12 (23) |
| R17 | 1kΩ → Q3 栅极 |
| R18 | 100kΩ 栅极下拉到 GND（图上与 R14 标号重叠） |

### VOUT-ADC
R9 470kΩ 与 R13 68kΩ 中点 → R11 100Ω → 网络 → U2 PA0 (5)；C9 100nF 对地。

### USB-VBUS-ADC
R10 470kΩ 与 R14 68kΩ 中点 → R12 100Ω → 网络 → U2 PC0 (3)；C10 100nF 对地；另有 C15 100nF 在该节点。

### IBUS-ADC
U1 INA180 OUT (1) → R16 100Ω → 网络 → U2 PA4 (9)；C11 10nF 对地。

### LCD_CS / LCD_RES / LCD_DC / LCD_SCK / LCD_SDA
| 网络 | LCD1 脚 | U2 脚 |
|---|---|---|
| LCD_CS | CS (7) | PA3 (8) |
| LCD_RES | RES (6) | PA1 (6) |
| LCD_DC | RS (5) | PA2 (7) |
| LCD_SCK | SCL (4) | PA5 (10) |
| LCD_SDA | SDA (3) | PA7 (12) |

### KEY-1 / KEY-2 / KEY-3
| 网络 | SW | 上拉 | 消抖 | U2 |
|---|---|---|---|---|
| KEY-1 | SW1 TS24CA | R19 10kΩ→3V3 | C16 10nF | PB3 (14) |
| KEY-2 | SW2 TS24CA | R20 10kΩ→3V3 | C17 10nF | PB4 (15) |
| KEY-3 | SW3 TS24CA | R21 10kΩ→3V3 | C18 10nF | PB6 (17) |

### LED
U2 PB8 (19) → LED1 LTST-C191KSKT → R37 100Ω → GND，高电平点亮。

### WS2812
U2 PB9 (20) → R26 100Ω → U6 DIN(4) → U6 DOUT(2) → U5 DIN → U5 DOUT → U4 DIN → U4 DOUT → U3 DIN → U3 DOUT 悬空。

### BEEP
Q4 S8050 基极（经 R24 1kΩ，R25 1kΩ 下拉到 GND）；网络 `BEEP` 为 Q4 基极驱动信号（图纸标在 R24 左端）。

### MCU-DCK / MCU-DIO
| 网络 | U2 | 串阻 | H1 |
|---|---|---|---|
| MCU-DCK | PC19/DCK (24) | R27 100Ω | 2 脚 |
| MCU-DIO | PC18/DIO (25) | R28 100Ω | 3 脚 |

### LOG-TX / LOG-RX
| 网络 | U2 | 串阻 | H2 |
|---|---|---|---|
| LOG-TX | PB10 (21) | R34 100Ω | 2 脚 |
| LOG-RX | PB11 (22) | R35 100Ω | 3 脚 |

## 2. 关键器件引脚 → 网络（拓扑用）

### USB1 · TYPE-C 16PIN 2MD(073)
| 焊盘 | 网络 | 焊盘 | 网络 |
|---|---|---|---|
| A1B12 | GND | B1A12 | GND |
| A4B9 | USB-VBUS | B4A9 | USB-VBUS |
| A5 | USB-CC1（R4 5.1kΩ↓） | B5 | USB-CC2（R1 5.1kΩ↓） |
| A6 | USB-DP（Dp1） | B6 | USB-DP（Dp2） |
| A7 | USB-DM（Dn1） | B7 | USB-DM（Dn2） |
| A8 | SBU1 → NC | B8 | SBU2 → NC |
| 13 | EH → GND | 14 | EH → GND |

### DCDC1（6 脚）
| 脚 | 名 | 网络 |
|---:|---|---|
| 1 | BST | C1 100nF → SW |
| 2 | GND | GND |
| 3 | FB | R5 124kΩ→3V3；R6 40.2kΩ→GND；C7 47pF→3V3 |
| 4 | EN | DCDC_EN（R3 510kΩ 上拉，SW3 接地） |
| 5 | IN | USB-VBUS |
| 6 | SW | L1 10µH、D1 阳极 |

### U1 · INA180A2IDBVR
| 脚 | 名 | 网络 |
|---:|---|---|
| 1 | OUT | → R16 100Ω → IBUS-ADC |
| 2 | GND | GND |
| 3 | IN+ | R7 10mΩ 的 USB-VBUS 侧 |
| 4 | IN− | R7 10mΩ 的 Q1 源极侧 |
| 5 | VS | 3V3（C12 100nF 去耦） |

> 图上符号的 IN+/IN− 脚号为 3/4；若按 TI 实际封装（SC70-5：1=OUT、2=GND、3=VS、4=IN−、5=IN+），
> 图纸符号脚号与实物存在差异，**画 PCB/调试时以原理图网络为准、以实物 datasheet 复核**。

### Q1 / Q2 · G200P04D3（P-MOS，8 脚）
| 脚 | 名 | 连接 |
|---:|---|---|
| 5,6,7,8 | D | 漏极：Q1 接 R7 之后的 VBUS 侧；Q2 接 VOUT |
| 1,2,3 | S | 源极：两管源极对接（背靠背） |
| 4 | G | 栅极：D2 10V 钳位 + R8 100kΩ 上拉 + R15 10kΩ 到 Q3 漏极 |

### Q3 · 2N7002 / Q4 · S8050 / D2 · BZT52C10
- Q3：栅极 ← R17 1kΩ ← `VOUT-EN`；栅极另有 100kΩ 下拉；源极 GND；漏极 → R15 10kΩ → Q1/Q2 栅极。
- Q4：基极 ← R24 1kΩ ← `BEEP`（R25 1kΩ 下拉）；发射极 GND；集电极 → R23 100Ω → BUZZER1（另一端 3V3，R22 1kΩ 上拉）。
- D2：接在 Q1/Q2 公共源极与公共栅极之间，做 G-S 电压钳位。

### LCD1 · N114-2413THBIG01-H13
| 脚 | 名 | 网络 | 脚 | 名 | 网络 |
|---:|---|---|---:|---|---|
| 1 | NC | 打叉 | 8 | GND | GND |
| 2 | NC | 打叉 | 9 | NC | 打叉 |
| 3 | SDA | LCD_SDA | 10 | VCC | 3V3 经 R2 100Ω，C6 100nF |
| 4 | SCL | LCD_SCK | 11 | LEDK | GND（背光常亮） |
| 5 | RS | LCD_DC | 12 | LEDA | 3V3（R2 之后） |
| 6 | RES | LCD_RES | 13 | GND | GND |
| 7 | CS | LCD_CS | | | |

### U3~U6 · WS2812C-V6
| 脚 | 名 | 连接 |
|---:|---|---|
| 1 | VDD | 3V3（每颗 100nF 去耦） |
| 2 | DOUT | 下一颗 DIN（U3 的 DOUT 悬空） |
| 3 | VSS | GND |
| 4 | DIN | 上一颗 DOUT（U6 由 R26 100Ω 来自 MCU PB9） |

### U2 · CH32X035G8U6
引脚-网络全表见 `SCHEMATIC_DESIGN.md` §3。

## 3. 电源通路速查（电流方向）

```
USB-C VBUS ──▶ [C2 4.7µF/C3 100nF] ──▶ R7 10mΩ ──▶ Q1 (D5-8→S1-3) ──▶ Q2 (S1-3→D5-8) ──▶ VOUT ──▶ CN1
                  │                        │                                                     ▲
                  └──▶ DCDC1 IN ──▶ 3V3    └── U1 INA180 (50V/V) ──▶ IBUS-ADC                 R9/R13 分压
                                                                                              ──▶ VOUT-ADC
             R10/R14 分压 ──▶ USB-VBUS-ADC
             DCDC_EN：R3 510kΩ 上拉（默认开）；SW3 按下关闭
             VOUT-EN：MCU PB12 → R17 1kΩ → Q3 2N7002 → R15 10kΩ → Q1/Q2 栅极（低=导通）
```
