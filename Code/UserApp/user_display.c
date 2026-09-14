/**
 * @file    user_display.c
 * @brief   用户显示任务实现
 *******************************************************************************
 * @note    利用 Protothread 协程实现周期性 LCD 显示刷新。
 *          LCD 显示内容包括：
 *          - Hall 传感器状态
 *          - 三相电流 + 母线电流 ADC 原始值
 *          - 4 个按键的按下状态
 *          - 4 个按键的上次事件类型
 *******************************************************************************
 */

#include "user_display.h"
#include "user_system.h"
#include "bsp_adc.h"
#include "bsp_hall.h"
#include "bsp_lcd.h"
#include "bsp_ws2812.h"
#include "user_button.h"
#include "user_foc.h"
#include "user_motor.h"
#include "st7789v/st7789v.h"

tDisplayDataDef tDisplayData;

#define DISPLAY_WS2812_LEVEL (25u)
#define DISPLAY_VBUS_R_TOP_OHM      (13600.0f)  /* R93 + R94: 6.8 kΩ + 6.8 kΩ */
#define DISPLAY_VBUS_R_BOTTOM_OHM   (2000.0f)   /* R95 */
#define DISPLAY_VBUS_DIVIDER_RATIO  ((DISPLAY_VBUS_R_TOP_OHM + DISPLAY_VBUS_R_BOTTOM_OHM) / DISPLAY_VBUS_R_BOTTOM_OHM)

static uint8_t u8PowerOnInitDone = 0u;
static uint8_t u8RunningScreenInitialized = 0u;



/**
 * @brief  初始化显示模块
 * @note   初始化 LCD 显示屏
 */
void UsrDisplayInit(void)
{
    BspLcdInit();
    BspWs2812Init();
}

/**
 * @brief  系统初始化状态下的显示处理
 * @note   设置 WS2812 为黄色（RGB 全亮），
 *         在 LCD 上显示 "FOC" 标题（仅首次执行）
 */
static void UsrDisplayInitState(void)
{
    u8RunningScreenInitialized = 0u;
    static uint8_t u8InitDone = 0u;

    tDisplayData.u8Color[0] = DISPLAY_WS2812_LEVEL;
    tDisplayData.u8Color[1] = DISPLAY_WS2812_LEVEL;
    tDisplayData.u8Color[2] = 0u;

    if (u8InitDone == 0u)
    {
        u8InitDone = 1u;
        LCD_ShowChineseTEST(0, 0, "FOC", BLACK, WHITE, 80,135, 0);
    }
}

/**
 * @brief  HSV 转 RGB
 * @param  u16Hue 色相 0..359
 * @param  u8Sat  饱和度 0..255
 * @param  u8Val  亮度 0..255
 * @param  pu8Rgb 输出 RGB 数组，索引 [R, G, B]
 */
static void UsrDisplayHsvToRgb(uint16_t u16Hue, uint8_t u8Sat, uint8_t u8Val, uint8_t *pu8Rgb)
{
    uint8_t u8Region, u8Remainder, u8P, u8Q, u8T;

    if (u8Sat == 0u)
    {
        pu8Rgb[0] = u8Val;
        pu8Rgb[1] = u8Val;
        pu8Rgb[2] = u8Val;
        return;
    }

    u8Region    = (uint8_t)(u16Hue / 60u);
    u8Remainder = (uint8_t)(u16Hue % 60u);
    u8P = (uint8_t)(((uint16_t)u8Val * (255u - u8Sat)) / 255u);
    u8Q = (uint8_t)(((uint16_t)u8Val * (255u - (uint16_t)u8Sat * u8Remainder / 60u)) / 255u);
    u8T = (uint8_t)(((uint16_t)u8Val * (255u - (uint16_t)u8Sat * (60u - u8Remainder) / 60u)) / 255u);

    switch (u8Region)
    {
        case 0u:  pu8Rgb[0] = u8Val; pu8Rgb[1] = u8T; pu8Rgb[2] = u8P; break;
        case 1u:  pu8Rgb[0] = u8Q; pu8Rgb[1] = u8Val; pu8Rgb[2] = u8P; break;
        case 2u:  pu8Rgb[0] = u8P; pu8Rgb[1] = u8Val; pu8Rgb[2] = u8T; break;
        case 3u:  pu8Rgb[0] = u8P; pu8Rgb[1] = u8Q; pu8Rgb[2] = u8Val; break;
        case 4u:  pu8Rgb[0] = u8T; pu8Rgb[1] = u8P; pu8Rgb[2] = u8Val; break;
        default:  pu8Rgb[0] = u8Val; pu8Rgb[1] = u8P; pu8Rgb[2] = u8Q; break;
    }
}

/**
 * @brief  上电状态下的显示处理
 * @note   WS2812 彩虹呼吸效果：
 *         - 彩虹周期约 3.6s（每 10ms 色相 +1）
 *         - 呼吸周期 2s（三角波亮度）
 *         在 LCD 上显示作者信息（仅首次执行）
 */
static void UsrDisplayPowerOnState(void)
{
    u8RunningScreenInitialized = 0u;
    static uint16_t u16Hue = 0u;
    static uint16_t u16BreathTick = 0u;
    uint8_t u8Brightness;
    uint8_t au8Rgb[3];

    /* 彩虹：每 10ms 色相 +1，360 个 tick = 3.6s 转一圈 */
    u16Hue = (u16Hue + 1u) % 360u;

    /* 呼吸：2s 周期三角波，亮度在 0..255 之间变化 */
    u16BreathTick = (u16BreathTick + 1u) % 200u;
    if (u16BreathTick < 100u)
    {
        u8Brightness = (uint8_t)((u16BreathTick * 255u) / 99u);
    }
    else
    {
        u8Brightness = (uint8_t)(((199u - u16BreathTick) * 255u) / 99u);
    }

    UsrDisplayHsvToRgb(u16Hue, 255u, u8Brightness/100, au8Rgb);
    tDisplayData.u8Color[0] = au8Rgb[0];
    tDisplayData.u8Color[1] = au8Rgb[1];
    tDisplayData.u8Color[2] = au8Rgb[2];

    if (u8PowerOnInitDone == 0u)
    {
        u8PowerOnInitDone = 1u;
        LCD_ShowChineseTEST(0, 0, "FOC", BLACK, WHITE, 80,135, 0);
        LCD_ShowString(0, 0, "YJRQZ777", BLACK, WHITE, 16, 0);
    }
}

/**
 * @brief  运行状态下的显示处理
 * @note   设置 WS2812 为绿色，按周期刷新 LCD：
 *         - 左列：电位器 ADC 值、Iq 参考值和目标值
 *         - 中列：按键 2 按下状态、运行计数器
 *         实际刷新间隔 100ms（累积显示周期后输出）
 */
static void UsrDisplayRunningState(void)
{
    static uint8_t u8TimeCount = 0u;
    tDqCurrentDef DqCurrent;

    u8PowerOnInitDone = 0u;
    if (u8RunningScreenInitialized == 0u)
    {
        LCD_Fill(0u, 0u, LCD_W, LCD_H, WHITE);
        u8RunningScreenInitialized = 1u;
    }
    tDisplayData.u8Color[0] = 0u;
    tDisplayData.u8Color[1] = DISPLAY_WS2812_LEVEL;
    tDisplayData.u8Color[2] = 0u;

    u8TimeCount++;
    if (u8TimeCount < (100u / USR_DISPLAY_TASK_INTERVAL_MS))
    {
        return;
    }
    u8TimeCount = 0u;

    DqCurrent = UsrFocGetDqCurrent();
    BspLcdBeginRefresh((uint8_t)E_SYS_STATE_RUNNING);
    /* Kp/Ki 蓝色显示在首行，速度环浮点参数 */
    BspLcdAddFloat(0u, 48u, UsrMotorGetSpeed(), 8u, BLACK);
    BspLcdAddFloat(0u, 72u, UsrMotorGetSpeedRef(), 8u, BLACK);
    // BspLcdAddFloat(0u, 96u, UsrMotorGetIqRef(), 8u, BLACK);
    BspLcdAddFloat(96u, 48u, DqCurrent.f32Q, 8u, BLACK);
    BspLcdAddUInt(96u, 72u, UsrMotorIsOverCurrentFault(), 1u, UsrMotorIsOverCurrentFault() != 0u ? RED : BLACK);
    /* Vbus: ADC 输入电压按 R93+R94/R95 实际分压比换算为实际电压 */
    BspLcdAddFloat(96u, 96u,
                   BspAdc2GetVoltage(E_BSP_ADC2_VBUS) * DISPLAY_VBUS_DIVIDER_RATIO, 8u, RED);
}
static void UsrDisplayOffState(void)
{
    u8RunningScreenInitialized = 0u;
}

/**
 * @brief  Protothread 显示协程任务
 * @return PT 状态码
 * @note   首次进入时执行初始化，之后循环等待显示周期后刷新显示
 */
uint16_t UsrDisplayTask(void)
{
    int8_t i = 0;
    tDisplayFunctionDataDef DisplayFunction[E_SYS_STATE_MAX] = {
        {E_SYS_STATE_INIT, UsrDisplayInitState},
        {E_SYS_STATE_POWER_ON, UsrDisplayPowerOnState},
        {E_SYS_STATE_OFF, UsrDisplayOffState},
        {E_SYS_STATE_RUNNING, UsrDisplayRunningState},
    };

    PT_BEGIN()
    {
        UsrDisplayInit();
    }

    while (1)
    {
        PT_WAIT_UNTIL(USR_DISPLAY_TASK_INTERVAL_MS / OS_TICK_MS);

        // 显示数据复位
        memset(&tDisplayData, 0, sizeof(tDisplayData));

        for (i = (sizeof(DisplayFunction) / sizeof(DisplayFunction[0])) - 1; i >= 0; i--)
        {
            if (tSysData.eState == DisplayFunction[i].eState)
            {
                DisplayFunction[i].pfFunction();
                break;
            }
        }
        BspLcdService((uint8_t)tSysData.eState);
        // SEGGER_RTT_printf(0, "%d,%d,%d\r\n", tSysData.eState,tSysData.u32OpenTimes, tSysData.u32PowerOnTimes);
        BspWs2812SetColor(tDisplayData.u8Color[0],
                                 tDisplayData.u8Color[1],
                                 tDisplayData.u8Color[2]);
        BspWs2812Show();              // 输出到灯
    }
    PT_END();
}
