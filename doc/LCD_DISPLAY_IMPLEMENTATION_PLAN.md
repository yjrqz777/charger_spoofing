# CH32X035 240×135 LCD 仪表界面实施计划

## 1. 任务目标

在现有 CH32X035G8U6 固件中增加一个轻量、静态布局的 240×135 横屏数据显示页面，显示：

- VBUS 电压
- VOUT 电压
- IBUS 电流
- 输入功率 POWER
- VOUT 开关状态

界面采用简洁无衬线风格。字体使用开源 **Inter SemiBold**，在PC端提前转换为精简的2bpp点阵字库，固件运行时不解析TTF。

本任务不引入复杂UI框架，不要求触摸、菜单、动画或页面切换。

## 2. 已知硬件与约束

- MCU：CH32X035G8U6
- Flash：64KB
- SRAM：20KB
- LCD：240×135，横屏
- 通信：沿用项目现有LCD/SPI驱动
- 推荐实现：裸机绘制，不引入LVGL
- 禁止分配RGB565整屏帧缓冲：`240 × 135 × 2 = 64800 bytes`，超过MCU SRAM
- 不修改现有ADC采样、VOUT控制和底层LCD驱动的功能逻辑，除非接口确实存在缺陷

开始编码前必须确认：

1. LCD控制器型号、旋转方向和坐标原点。
2. 现有设置绘制窗口、填充矩形和SPI发送数据的函数。
3. VBUS、VOUT、IBUS采样结果当前使用的单位和滤波方式。
4. VOUT-EN当前的软件状态变量或GPIO读取接口。

## 3. 数据定义

界面层不要使用浮点数。统一传入工程单位整数：

```c
typedef struct {
    uint32_t vbus_mv;          // VBUS，单位mV
    uint32_t vout_mv;          // VOUT，单位mV
    uint32_t ibus_ma;          // IBUS，单位mA
    uint32_t power_mw;         // 输入功率，单位mW
    bool output_enabled;       // VOUT开关命令状态
    bool measurements_valid;   // ADC结果是否有效
} UiPowerData;
```

POWER定义为输入功率，因为当前只有IBUS采样：

```c
power_mw = (uint32_t)(((uint64_t)vbus_mv * ibus_ma + 500U) / 1000U);
```

不要把它称为输出功率；没有IOUT测量时无法得到真实输出功率。

数值显示格式：

| 项目 | 格式 | 示例 |
|---|---|---|
| VBUS | `xx.xx V` | `12.08 V` |
| VOUT | `xx.xx V` | `11.96 V` |
| IBUS | `x.xx A` | `1.22 A` |
| POWER | `xx.x W` | `14.7 W` |
| OUTPUT | `ON` / `OFF` | `ON` |

实现专用整数格式化函数，不依赖浮点版`printf/snprintf`。数值无效时显示`--.--`或`---`。

## 4. 页面布局

屏幕坐标范围：`x=0..239`，`y=0..134`。外围留8px左右边距。

### 4.1 区域坐标

| 区域 | x | y | w | h | 内容 |
|---|---:|---:|---:|---:|---|
| 顶部状态栏 | 8 | 7 | 224 | 17 | 左侧标题，右侧连接状态 |
| VBUS卡片 | 8 | 30 | 109 | 54 | VBUS标签与大数字 |
| VOUT卡片 | 123 | 30 | 109 | 54 | VOUT标签与大数字 |
| POWER区域 | 8 | 90 | 85 | 37 | 功率 |
| IBUS区域 | 99 | 90 | 61 | 37 | 电流 |
| OUTPUT区域 | 166 | 90 | 66 | 37 | 开关状态 |

所有区域必须限制在各自矩形内，不能依赖整屏裁剪掩盖越界。

### 4.2 顶部状态栏

- 左侧显示绿色4px圆点和`POWER MONITOR`。
- 右侧只显示真实存在的状态。
- 如果固件没有USB-PD协议状态，不允许固定显示`PD READY`；改为`ONLINE`、`USB POWER`或留空。
- y=23处绘制一条1px分隔线。

### 4.3 数字排版

- VBUS/VOUT：24px Inter SemiBold。
- POWER/IBUS：16px Inter SemiBold。
- 标签、单位和顶部状态：8px Inter SemiBold。
- 数值右侧单位使用8px字库并采用弱化灰色。
- `0..9`必须采用相同字符前进宽度，避免数据变化时左右跳动。
- VBUS/VOUT卡片中的数字建议左对齐，固定基线。

## 5. RGB565颜色

| 用途 | RGB888 | RGB565 |
|---|---|---|
| 页面背景 | `#070B0F` | `0x0041` |
| 卡片背景 | `#0C1218` | `0x0883` |
| 边框/分隔线 | `#1B2832` | `0x1946` |
| 次要文字 | `#70818D` | `0x7411` |
| 主要文字 | `#EEF6F8` | `0xEFBF` |
| VBUS青色 | `#45D8E6` | `0x46DC` |
| VOUT/ON绿色 | `#62E69A` | `0x6733` |
| 功率橙色 | `#FFC766` | `0xFE2C` |
| 开关底色 | `#214737` | `0x2226` |

若LCD驱动要求字节交换，只在底层发送函数中统一处理，不要改变上表逻辑颜色常量。

## 6. 字体资源生成

### 6.1 字体来源

- 字体：Inter SemiBold静态TTF
- 上游：https://github.com/rsms/inter
- 保留上游`LICENSE.txt`，不要把整套TTF编译进固件

### 6.2 生成三套精简点阵

| 字库 | 像素高度 | 建议字符子集 |
|---|---:|---|
| `font_inter_24` | 24 | `0123456789.-` |
| `font_inter_16` | 16 | `0123456789.-` |
| `font_inter_8` | 8 | 界面实际使用的大写字母、数字、空格、点、短横线及`VAW` |

转换工具放在仓库的`tools/font_converter/`，生成结果放入`src/ui/fonts/`。生成操作必须可重复，不接受手工逐字复制字模。

点阵格式：

- 2bpp灰度/透明度，每像素取值0..3。
- 每字节打包4个像素。
- 字形位图按行存储。
- 每个字号单独生成，不在MCU端缩放字模。
- 只生成用到的字符，控制Flash占用。

建议字形描述结构：

```c
typedef struct {
    uint32_t bitmap_offset;
    uint8_t width;
    uint8_t height;
    int8_t x_offset;
    int8_t y_offset;
    uint8_t advance;
    uint32_t codepoint;
} UiGlyph;

typedef struct {
    const uint8_t *bitmap;
    const UiGlyph *glyphs;
    uint16_t glyph_count;
    uint8_t line_height;
    uint8_t baseline;
} UiFont;
```

转换完成后输出每套字库的总字节数。若三套字库和渲染代码占用明显过大，优先减少字符子集，其次把8px标签字体改成1bpp，不要删减24px数字质量。

## 7. 字体渲染器

新增轻量渲染模块，建议接口：

```c
void UiFont_DrawText(int16_t x, int16_t baseline_y,
                     const UiFont *font,
                     uint16_t fg, uint16_t bg,
                     const char *text);

uint16_t UiFont_MeasureText(const UiFont *font, const char *text);
```

2bpp像素混合：

- alpha=0：输出背景色。
- alpha=3：输出前景色。
- alpha=1/2：分别按1/3、2/3混合RGB565前景与背景。
- 可以逐行使用小型临时RGB565缓冲区，禁止申请整屏缓冲区。
- LCD窗口应按整个字符串或单个字形矩形设置，避免逐像素发送SPI命令。

为了适配比例字体，字符定位使用`advance`；数字`0..9`的`advance`在生成阶段统一为最大数字宽度。

## 8. 建议代码结构

根据现有工程目录调整名称，但职责必须分开：

```text
src/
├── drivers/
│   └── lcd.*                 # 现有底层驱动，尽量不改
└── ui/
    ├── ui_dashboard.c        # 页面布局、静态与动态绘制
    ├── ui_dashboard.h
    ├── ui_font.c             # 点阵文字渲染
    ├── ui_font.h
    └── fonts/
        ├── font_inter_8.c
        ├── font_inter_8.h
        ├── font_inter_16.c
        ├── font_inter_16.h
        ├── font_inter_24.c
        └── font_inter_24.h
tools/
└── font_converter/
    ├── generate_fonts.py
    └── README.md
```

建议页面接口：

```c
void UiDashboard_Init(void);
void UiDashboard_SetData(const UiPowerData *data);
void UiDashboard_Refresh(void);
```

- `Init()`清屏并只绘制一次背景、卡片、边框、标签和单位。
- `SetData()`复制新数据，不直接操作SPI。
- `Refresh()`比较新旧数据，只重绘变化的动态区域。

## 9. 刷新策略

- 页面刷新建议5Hz，ADC采样可使用更高频率。
- 静态背景只在初始化、屏幕唤醒或完整重绘时发送。
- 每个数值拥有固定刷新矩形；先用对应背景色清除，再绘制新值。
- 只有格式化后的字符串发生改变才刷新该矩形。
- OUTPUT状态只有发生变化时刷新。
- 不在ADC中断中刷新LCD；在主循环或定时任务中执行。
- 避免每次刷新调用浮点运算、`malloc`或大尺寸栈数组。

建议维护：

```c
typedef struct {
    char vbus[8];
    char vout[8];
    char ibus[8];
    char power[8];
    bool output_enabled;
    bool valid;
} UiDashboardCache;
```

## 10. 开关状态显示

- ON：绿色文字，绿色圆点位于开关槽右侧。
- OFF：灰色文字，灰色圆点位于开关槽左侧。
- 状态来源优先使用软件维护的VOUT-EN命令状态。
- 如果项目能够同时检测VOUT，可选增加FAULT：命令为ON但VOUT低于合理阈值并持续一定时间时显示红色`FAULT`。
- 未实现真实故障判断前，不要根据一次ADC抖动显示FAULT。

## 11. Agent执行顺序

1. 审阅现有工程，记录LCD底层API、屏幕方向、采样数据接口和VOUT-EN接口。
2. 给出将要新增或修改的文件清单，避免重写无关模块。
3. 加入字体转换脚本并生成三套精简Inter字库。
4. 实现2bpp字体测量和绘制函数，先用测试字符串验证基线、间距和颜色。
5. 实现静态页面布局。
6. 实现整数格式化与动态区域局部刷新。
7. 接入真实VBUS、VOUT、IBUS和VOUT-EN状态。
8. 编译并报告Flash、RAM增量以及所有警告。
9. 上板验证；如无硬件，至少提供可调用的测试数据入口和关键SPI调用证据。

## 12. 验收标准

- 工程零错误编译，新增代码无编译警告。
- 屏幕方向正确，所有内容位于240×135范围内。
- 五项数据全部显示，单位正确。
- POWER使用`VBUS × IBUS`计算，并使用64位中间值避免溢出。
- 不使用浮点格式化，不分配整屏缓冲区，不使用动态内存。
- 数字变化时位置稳定，无明显左右跳动。
- 单个数据变化时只刷新对应矩形，不整屏闪烁。
- ON/OFF显示与实际VOUT-EN命令一致。
- 字体有抗锯齿，24px数字清晰，8px标签可辨认。
- 字库只包含所需字符，并报告字库与总代码体积。
- 现有ADC、USB、按键、WS2812、VOUT控制和调试接口功能不得回归。

## 13. Agent需要在完成时交付

1. 修改文件列表及每个文件用途。
2. 字体来源、许可证和可重复生成命令。
3. 最终Flash/SRAM占用及与修改前的差值。
4. 页面刷新频率和最坏情况下的SPI传输量。
5. 实机照片或模拟输出；无法实机测试时明确说明。
6. 未完成项、硬件依赖和需要人工确认的问题。

## 14. 禁止事项

- 禁止直接把TTF文件放进MCU并尝试运行时解析。
- 禁止引入LVGL或其他大型UI框架，除非先说明资源增量并获得确认。
- 禁止使用RGB565整屏帧缓冲。
- 禁止为了显示小数而启用浮点`printf`。
- 禁止每次循环无条件整屏刷新。
- 禁止把未检测的协议状态硬编码为`READY`。
- 禁止修改与LCD任务无关的引脚、时钟、USB和电源控制配置。

