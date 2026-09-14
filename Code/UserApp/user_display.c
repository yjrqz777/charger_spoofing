/**
 * @file    user_display.c
 * @brief   用户显示任务实现 — 单页实时数据面板
 *******************************************************************************
 * @note    屏幕：240x135 横屏（ST7789V，SPI1 轮询发送）
 *
 *          界面布局（24 号字，每行 24 像素）：
 *            +--------------------------------------------+
 *            | pd-spoofing                EN:OFF          |  第 0 行（16 号字）
 *            | VBUS   12.34 V                             |  第 1 行 y=28
 *            | VOUT   12.30 V                             |  第 2 行 y=56
 *            | IBUS    1.234 A                            |  第 3 行 y=84
 *            | POUT   15.18 W                             |  第 4 行 y=106
 *            +--------------------------------------------+
 *
 *          刷新策略（配合 BspLcdService 的分时字段状态机）：
 *            - 每 100ms 采样一次 ADC；
 *            - 每 100ms 组装一帧显示请求，由 BspLcdService() 每个时间片输出一条；
 *            - 整屏填充只在进入状态时执行一次（阻塞约 200ms）。
 *******************************************************************************
 */

#include "user_display.h"
#include "bsp_lcd.h"
#include "bsp_adc.h"
#include "bsp_board.h"
#include "bsp_button.h"
#include "bsp_spi.h"
#include "st7789v/st7789v.h"

/* ---- 布局常量 ---- */
#define DISPLAY_ROW_HEIGHT      (28u)    /**< 数据行行高 */
#define DISPLAY_ROW0_Y          (0u)     /**< 顶栏 Y */
#define DISPLAY_ROW1_Y          (28u)    /**< 第 1 行 Y */
#define DISPLAY_ROW2_Y          (56u)    /**< 第 2 行 Y */
#define DISPLAY_ROW3_Y          (84u)    /**< 第 3 行 Y */
#define DISPLAY_ROW4_Y          (106u)   /**< 第 4 行 Y */

#define DISPLAY_LABEL_X         (4u)     /**< 标签 X */
#define DISPLAY_VALUE_X         (72u)    /**< 数值 X（24 号字，6 字符宽 72px） */
#define DISPLAY_EN_X            (168u)   /**< 开关状态 X */

/** @brief Runtime diagnostic log interval in milliseconds. */
#define DISPLAY_DIAGNOSTIC_INTERVAL_MS (1000u)

/** @brief 进入 RUNNING 前，整屏只填充一次 */
static uint8_t s_u8ScreenCleared = 0u;

/** @brief 顶栏是否已经画过（静态内容只画一次，避免每帧重复输出） */
static uint8_t s_u8StaticDrawn = 0u;

/** @brief 采样与刷新的时间累加器（单位 ms） */
static uint16_t s_u16SampleAcc = 0u;
static uint16_t s_u16RefreshAcc = 0u;
static uint16_t s_u16DiagnosticAcc = 0u;

/**
 * @brief  初始化显示模块
 * @note   初始化 LCD 与 SPI1。包含初始化延时，仅上电时执行一次。
 */
void UsrDisplayInit(void)
{
    printf("[DISPLAY] initialization begin\r\n");
    BspLcdInit();
    printf("[DISPLAY] initialization end, spi_error=%u\r\n",
           (unsigned int)BspSpiHasError());

    s_u8ScreenCleared = 0u;
    s_u8StaticDrawn   = 0u;
    s_u16SampleAcc    = 0u;
    s_u16RefreshAcc   = 0u;
    s_u16DiagnosticAcc = 0u;
}

/**
 * @brief  绘制静态内容（顶栏与行标签）
 * @note   只在状态变化后执行一次，走阻塞输出（字符串接口），
 *         此时没有排队字段，不会与字段状态机冲突。
 */
static void UsrDisplayDrawStatic(void)
{
    BspLcdShowString(DISPLAY_LABEL_X, DISPLAY_ROW0_Y, "pd-spoofing", BLACK, WHITE, 16u, 0u);
    BspLcdShowString(DISPLAY_LABEL_X, DISPLAY_ROW1_Y, "VBUS", BLACK, WHITE, 24u, 0u);
    BspLcdShowString(DISPLAY_LABEL_X, DISPLAY_ROW2_Y, "VOUT", BLACK, WHITE, 24u, 0u);
    BspLcdShowString(DISPLAY_LABEL_X, DISPLAY_ROW3_Y, "IBUS", BLACK, WHITE, 24u, 0u);
    BspLcdShowString(DISPLAY_LABEL_X, DISPLAY_ROW4_Y, "POUT", BLACK, WHITE, 16u, 0u);
}

/**
 * @brief  刷新一帧实时数据
 * @param[in] ptData  采样数据
 * @note   本函数只写请求队列，实际输出由 BspLcdService() 分时间片推进。
 */
static void UsrDisplayRefreshFrame(const tBspAdcDataDef *ptData)
{
    BspLcdBeginRefresh((uint8_t)tSysData.eState);

    /* 第 1 行：输入母线电压（3 位整数 + 2 位小数，共 6 字符） */
    BspLcdAddFloat(DISPLAY_VALUE_X, DISPLAY_ROW1_Y, ptData->f32Voltage, 6u, 2u, BLACK);

    /* 第 2 行：输出电压 */
    BspLcdAddFloat(DISPLAY_VALUE_X, DISPLAY_ROW2_Y, ptData->f32Vout, 6u, 2u, BLACK);

    /* 第 3 行：输出电流（3 位小数，可显示到 9.999A） */
    BspLcdAddFloat(DISPLAY_VALUE_X, DISPLAY_ROW3_Y, ptData->f32Current, 6u, 3u, BLACK);

    /* 第 4 行：输出功率（2 位小数，6 字符宽；y=106 + 24 = 130 < 135） */
    BspLcdAddFloat(DISPLAY_VALUE_X, DISPLAY_ROW4_Y, ptData->f32Power, 6u, 2u, BLACK);

    /* 顶栏右侧：输出开关状态 */
    if (BspBoardGetVoutEnable() != 0u)
    {
        BspLcdAddString(DISPLAY_EN_X, DISPLAY_ROW0_Y, "EN:ON ", BLACK, WHITE, 16u);
    }
    else
    {
        BspLcdAddString(DISPLAY_EN_X, DISPLAY_ROW0_Y, "EN:OFF", BLACK, WHITE, 16u);
    }
}

/**
 * @brief  INIT 状态显示处理（上电初期，屏幕尚未初始化完成）
 */
static void UsrDisplayInitState(void)
{
    /* INIT 仅持续约 100ms，此阶段不放任何绘制，避免与 LCD 初始化竞争 */
}

/**
 * @brief  POWER_ON 状态显示处理：开机页
 * @note   LCD 初始化后先清屏，再显示产品名与版本号，保持约 1s。
 */
static void UsrDisplayPowerOnState(void)
{
    if (s_u8ScreenCleared == 0u)
    {
        BspLcdClearScreen(WHITE);
        s_u8ScreenCleared = 1u;
    }

    if (s_u8StaticDrawn == 0u)
    {
        BspLcdShowString(40u,  30u, "pd-spoofing", RED,   WHITE, 24u, 0u);
        BspLcdShowString(40u,  66u, "CH32X035G8U", BLUE,  WHITE, 16u, 0u);
        BspLcdShowString(40u,  92u, "FW  V0.1",    BLACK, WHITE, 16u, 0u);
        s_u8StaticDrawn = 1u;
    }
}

/**
 * @brief  RUNNING 状态显示处理：实时数据面板
 * @note   采样每 USR_DISPLAY_SAMPLE_MS 一次；
 *         刷新每 USR_DISPLAY_REFRESH_MS 发起一帧（由状态机异步输出）。
 */
static void UsrDisplayRunningState(void)
{
    const tBspAdcDataDef *ptData;

    /* 进入 RUNNING 的第一次：清屏 + 画静态内容 */
    if (s_u8ScreenCleared == 0u)
    {
        BspLcdClearScreen(WHITE);
        s_u8ScreenCleared = 1u;
        s_u8StaticDrawn   = 0u;
    }

    if (s_u8StaticDrawn == 0u)
    {
        UsrDisplayDrawStatic();
        s_u8StaticDrawn = 1u;
    }

    /* 采样累加 */
    s_u16SampleAcc += USR_DISPLAY_TASK_INTERVAL_MS;
    if (s_u16SampleAcc >= USR_DISPLAY_SAMPLE_MS)
    {
        s_u16SampleAcc = 0u;
        BspAdcUpdateAll();
    }

    /* 刷新累加：周期到时提交一帧字段请求 */
    s_u16RefreshAcc += USR_DISPLAY_TASK_INTERVAL_MS;
    if (s_u16RefreshAcc >= USR_DISPLAY_REFRESH_MS)
    {
        s_u16RefreshAcc = 0u;

        ptData = BspAdcGetData();
        UsrDisplayRefreshFrame(ptData);
    }

    s_u16DiagnosticAcc += USR_DISPLAY_TASK_INTERVAL_MS;
    if (s_u16DiagnosticAcc >= DISPLAY_DIAGNOSTIC_INTERVAL_MS)
    {
        s_u16DiagnosticAcc = 0u;
        ptData = BspAdcGetData();
        printf("[RUN] ADC vbus=%u vout=%u ibus=%u keys=0x%02x spi_error=%u\r\n",
               (unsigned int)ptData->u16Raw[E_BSP_ADC_VBUS],
               (unsigned int)ptData->u16Raw[E_BSP_ADC_VOUT],
               (unsigned int)ptData->u16Raw[E_BSP_ADC_IBUS],
               (unsigned int)BspButtonGetRawMask(),
               (unsigned int)BspSpiHasError());
    }
}

/**
 * @brief  OFF 状态显示处理（预留）
 */
static void UsrDisplayOffState(void)
{
    s_u8ScreenCleared = 0u;
    s_u8StaticDrawn   = 0u;
}

/**
 * @brief  Protothread 显示协程任务
 * @return PT 状态码
 * @note   首次进入执行初始化，之后每 USR_DISPLAY_TASK_INTERVAL_MS
 *         按当前系统状态刷新显示，并推进 LCD 字段状态机。
 */
uint16_t UsrDisplayTask(void)
{
    int8_t i = 0;
    static const struct
    {
        eSysStateDef eState;
        void (*pfFunction)(void);
    } atDisplayFunction[E_SYS_STATE_MAX] =
    {
        {E_SYS_STATE_INIT,     UsrDisplayInitState},
        {E_SYS_STATE_POWER_ON, UsrDisplayPowerOnState},
        {E_SYS_STATE_OFF,      UsrDisplayOffState},
        {E_SYS_STATE_RUNNING,  UsrDisplayRunningState},
    };

    PT_BEGIN()
    {
        UsrDisplayInit();
    }

    while (1)
    {
        PT_WAIT_UNTIL(USR_DISPLAY_TASK_INTERVAL_MS / OS_TICK_MS);

        /* 状态改变时复位"静态内容已画"标志，强制重画界面骨架 */
        {
            static eSysStateDef eLastState = E_SYS_STATE_MAX;

            if (eLastState != tSysData.eState)
            {
                printf("[DISPLAY] state=%u\r\n", (unsigned int)tSysData.eState);
                if (eLastState != E_SYS_STATE_MAX)
                {
                    s_u8ScreenCleared = 0u;
                    s_u8StaticDrawn = 0u;
                }
                eLastState = tSysData.eState;
            }
        }

        for (i = (int8_t)(sizeof(atDisplayFunction) / sizeof(atDisplayFunction[0])) - 1; i >= 0; i--)
        {
            if (tSysData.eState == atDisplayFunction[i].eState)
            {
                atDisplayFunction[i].pfFunction();
                break;
            }
        }

        /* 推进 LCD 字段状态机，每个时间片最多轮询输出一个字段。 */
        BspLcdService((uint8_t)tSysData.eState);
    }
    PT_END();
}
