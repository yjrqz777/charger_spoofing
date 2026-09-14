# pd-spoofing — CH32X035G8U6 USB-PD 取电板固件

从 USB-C 充电器取电，通过 CC 做 **USB PD 受电协商**（可选择固定档位），把 VBUS 经背靠背 MOS 开关从 XT30 输出，并对输出电压/电流做采样与显示。

- 主控：**CH32X035G8U6**（QFN-28，RISC-V，48MHz HSI）
- 工具链：MounRiver Studio 2（`riscv-none-embed-gcc`，`-Os`，无硬件浮点）
- 硬件资料见 [`doc/schematic/`](doc/schematic/)（原理图落盘的文本版，含逐脚网络表与 BOM）

---

## 1. 功能一览

| 功能 | 说明 |
|---|---|
USB-PD 受电 | CC1/CC2 检测 → 收 SrcCap → 申请指定固定 PDO → 建立契约 |
电压/电流采样 | VBUS、VOUT 分压（≈1:7.9，满量程约 26V）；IBUS 经 INA180（50V/V）+ 10mΩ 采样（满量程约 6.6A） |
输出开关 | PB12 (VOUT-EN) → Q3 → Q1/Q2 背靠背 P-MOS，上电默认**关断** |
显示 | 240×135 ST7789V（SPI1 + DMA），单页数据面板 + 开机页 |
交互 | 3 按键（低有效）：KEY1 降档 / KEY2 升档 / KEY3 切换输出 |
灯效 | 4× WS2812C（TIM1_CH1 PWM + DMA1_CH5）颜色渐变 |
日志 | USART1 @115200 → H2 排针（PB10），所有诊断都走这里 |

---

## 2. 目录结构

```
Code/
  main.h             引脚映射「唯一真相来源」+ 硬件换算系数
  Task.h             Protothread 时间片调度器（PT_BEGIN/PT_WAIT_UNTIL/PT_END）
  user_config.h      功能开关：USER_PD_ENABLE / USER_LCD_ENABLE / 申请档位
  UserApp/           应用层：任务、状态机、界面
  UserBsp/           板级驱动：LCD/SPI/ADC/按键/WS2812/USBPD
    st7789v/         ST7789V 驱动 + 字库
User/
  main.c             入口：初始化 + 注册任务
  ch32x035_it.c      异常向量（HardFault 直接复位）
  system_ch32x035.c  时钟（48MHz HSI，Flash 2 等待周期）
Ld/Link.ld           FLASH 62K / RAM 20K / 栈 2K（栈在 RAM 顶）
doc/                 原理图资料（详见 doc/schematic/README.md）
```

---

## 3. 任务调度

TIM3 每 **1ms** 中断一次，递减 `PT_TICK[]`；主循环用 `PT_TASK_REG(Rank, Func)` 轮询，
倒计时归零才调用对应任务函数：

| Rank | 任务 | 周期 | 职责 |
|---:|---|---:|---|
0 | `UsrDisplayTask` | 10ms | 状态页/数据面板刷新（可被 `USER_LCD_ENABLE` 关闭） |
1 | `UsrButtonTask` | 5ms | 派发按键事件回调（按键消抖在 TIM3 中断里做） |
2 | `UsrSystemTask` | 5ms | 系统状态机 `INIT→POWER_ON→RUNNING` |
3 | `UsrTimeTask` | 10ms | 上电/开机计时，触发 `INIT→POWER_ON` |
4 | `UsrWs2812Task` | 10ms | 灯效渐变 |
5 | `UsrPdTask` | 1ms | PD 状态机（含 `[RUN]` 诊断输出） |

---

## 4. 引脚映射

| 功能 | 引脚 | 备注 |
|---|---|---|
LCD RES / DC / CS | PA1 / PA2 / PA3 | 推挽输出 |
LCD SCK / SDA | PA5 / PA7 | SPI1 单线发送，**Mode 2**，分频 4（12MHz） |
ADC | PA0 = VOUT，PA4 = IBUS，PC0 = VBUS | 模拟输入 |
PD CC1 / CC2 | PC14 / PC15 | USBPD PHY，浮空输入 + `USBPD_IN_HVT \| USBPD_PHY_V33` |
按键 KEY1 / KEY2 / KEY3 | PB3 / PB4 / PB6 | 上拉输入，低有效 |
LED / WS2812 | PB8 / PB9 | WS2812 用 TIM1_CH1 |
日志 TX / RX | PB10 / PB11 | USART1 @115200 |
输出使能 VOUT-EN | PB12 | 高 = 导通，上电默认低 |

> 改引脚只需改 `Code/main.h`，驱动层全部引用这些宏。

---

## 5. 关键实现注意（踩过的坑，改代码前务必读）

### 5.1 主循环不能被长时间阻塞

PD 协议有硬时序：源端发 `ACCEPT` 后大约 **10~30ms** 就发 `PS_RDY`，Sink 必须在
**500ms** 内响应，否则链路复位。所以**任何单次占用 CPU 超过 ~10ms 的绘制都会导致协商失败**。

因此 LCD 的所有输出都走 **DMA 分段异步**：

| 操作 | 实现 | 单次 CPU 占用 |
|---|---|---|
整屏填充 | `LCD_Fill()` 只登记窗口与状态，`LCD_FillRowDma()` 每次推 ≤9 行 | ~1ms |
字符串 | `LCD_ShowStringChunkDma()` 每块 ≤8 字符（`LCD_DMA_TEXT_CHUNK_CHARS`） | ~1ms |
数字 | `LCD_ShowIntNumAsync()` / `LCD_ShowFloatNumAsync()` | ~1ms |

`BspLcdService()` 每个时间片推进一块，绝不整屏同步画。

### 5.2 字段队列容量必须够

所有绘制操作先进 `s_atRequestedOp[BSP_LCD_FIELD_MAX]` 队列。
**字符串现在也占槽位，且长字符串按每 8 字符占 1 个槽**（`"pd-spoofing"` = 2 槽）。
最坏一帧（填充 1 + 12 静态 + 4 数字 + 3 顶栏）需要 **20** 槽。

队列满时操作会被**丢弃**，并累加 `BspLcdGetDroppedOps()`。
`[RUN]` 日志里的 **`lcd_drop` 必须是 0**，不为 0 就说明界面有内容没画出来。

### 5.3 PD 驱动来自 WCH 官方例程

`Code/UserBsp/bsp_usb_pd.c` 是 `tools/EVT/EXAM/USBPD/USBPD_SNK/User/PD_Process.c`
的移植（函数一一对应，文件头有映射表）。改动时请对照例程，**例程里的寄存器时序不要
"顺手优化"**。两点已知保留项：

- `BspUsbPdSendPhy()` 里等待 `IF_TX_END` 是**无超时**死循环（例程原样）
- `USBPD_IRQHandler()` 里有 `printf`（例程原样）

版本标识：串口应打印 `[PD] driver v2-example-port`。**没有这行说明烧的不是这份代码。**

### 5.4 编译前必须让 IDE 重新加载

MounRiver 的编辑器**会用它内存里的旧内容覆盖磁盘文件**。用外部编辑器改完代码后，
必须先让 IDE 重新加载（或重启 IDE）再 Build，否则改动会被无声丢弃、白烧一轮。
构建后可用 `obj/` 里 `.o` 文件的字符串验证实际编进去的是哪一版。

---

## 6. 编译与烧录

1. MounRiver Studio 2 打开工程（`.cproject` / `.wvproj` 已在根目录）
2. **Project → Clean**，再 Build（增量编译在改动头文件时不可靠）
3. 通过 H1（SWD）烧录；日志接 H2（USART1，115200 8N1）

`obj/` 下产出 `CH32X035G8U.hex` / `.elf` / `.map`。

---

## 7. 诊断日志

正常建立契约时，串口应依次出现：

```
[BOOT] pd-spoofing start
[BOOT] SYSCLK=48000000 Hz ChipID=03560631
[TICK] TIM3 1ms started
[PD] driver v2-example-port           ← 版本标识
[PD] sink initialized: CC1=PC14 CC2=PC15 request=PDO1
[LCD] init complete, spi_error=0
[PD] CC2 SRC Connect                  ← CC 检测成功
[PD] SrcCap: 5 fixed PDO(s) (tx=0 rx=1)
[PD] request PDO1: 5000 mV 3000 mA
[PD] ACCEPT (tx=1 rx=2)
[PD] PS_RDY: contract 5000 mV 3000 mA ← 契约建立
[RUN] PD=1/5 5000mV 3000mA loops=1000/s cc=2 lcd_drop=0
```

每秒一条的 `[RUN]` 是最重要的健康指标：

| 字段 | 正常值 | 含义 |
|---|---|---|
`loops` | ≈ **1000** | 本秒内 PD 任务被调度次数（理论 1000）。偏低说明主循环被拖慢 |
`lcd_drop` | **0** | 因 LCD 队列满而被丢弃的绘制操作数 |
`PD=x/y` | `1/5` | 当前档位 / 电源声明的固定档位数 |
`cc` | 1 或 2 | 实际使用哪条 CC |

常用开关（`Code/user_config.h`）：

| 宏 | 说明 |
|---|---|
`USER_PD_ENABLE` | 关闭后完全不碰 USBPD 外设 |
`USER_LCD_ENABLE` | 置 0 可摘掉整个显示子系统，单独验证 PD 实时性 |
`USER_PD_REQUEST_PDO_INDEX` | 申请第几组固定 PDO（1 = 5V，最安全） |
`LCD_IO_STATIC_TEST_ENABLE` | 把 5 根 LCD 信号线钉在固定电平，用于查线 |

---

## 8. 已知硬件关注点

1. **3V3 来自 VBUS**（DCDC1）。MCU 没有独立供电，**VBUS 一掉就整片复位**。PD 换档
   瞬间源端会先回落到 vSafe5V，若 DCDC 输入范围不够会导致重启 —— 排查时优先量 VBUS 与 3V3。
2. **DCDC1 拓扑存疑**：原理图里 L1 在 SW→3V3、D1 从 SW 对地，需对照所选芯片规格书确认
   （见 `doc/schematic/SCHEMATIC_DESIGN.md` §8.1）。
3. **CC 与 5.1kΩ 下拉并联**：图纸未画耦合电路，需确认 PHY 能否直接挂在 CC 上收 BMC。
   若 CC 检测结果在 CC1/CC2 之间跳动，通常是 CC 比较器被开关噪声干扰。
4. 背光 `LEDK` 直接接地，**无调光能力**，且可作为"3V3 是否有电"的直观指示。

---

## 9. 相关文档

| 文档 | 内容 |
|---|---|
[`doc/schematic/README.md`](doc/schematic/README.md) | 原理图资料索引与关键结论速览 |
[`doc/schematic/SCHEMATIC_DESIGN.md`](doc/schematic/SCHEMATIC_DESIGN.md) | 设计说明、功率路径、逐脚表、读图存疑项 |
[`doc/schematic/NETLIST.md`](doc/schematic/NETLIST.md) | 网络↔引脚连接表 |
[`doc/schematic/COMPONENTS.md`](doc/schematic/COMPONENTS.md) | BOM |
[`../CH32X035_1ms_tick_analysis.md`](../CH32X035_1ms_tick_analysis.md) | 1ms 节拍（TIM3）选型与配置分析 |
