/**
 * @file    user_button.c
 * @brief   用户按键管理实现
 *******************************************************************************
 * @note    基于 multi-button 库的 3 按键管理（本板只有 KEY1/KEY2/KEY3）。
 *          支持单击、双击、长按、重复触发等事件。
 *
 *          事件绑定（需求确认）：
 *            - KEY1 单击  ：无（页面只有一页，预留）
 *            - KEY1 长按  ：切换输出开关 VOUT-EN
 *            - KEY2 单击  ：数值调整减（预留）
 *            - KEY3 单击  ：数值调整加（预留）
 *******************************************************************************
 */

#include "user_button.h"
#include "user_button_fun.h"
#include "bsp_button.h"

/** @brief 3 个按键的 Button 结构体实例 */
static Button tButtonOne;
static Button tButtonTwo;
static Button tButtonThree;

/** @brief 初始化完成标志 */
static uint8_t u8ButtonInitialized = 0u;

/** @brief 各按键上次触发事件记录 */
static uint8_t au8ButtonLastEvent[USER_BUTTON_COUNT] = {0u};

/**
 * @brief  根据 ID 获取按键结构体指针
 * @param[in] button_id  按键 ID（1~3）
 * @return Button 结构体指针，无效 ID 返回 NULL
 */
static Button *UsrButtonGetHandle(uint8_t u8ButtonId)
{
    switch (u8ButtonId) {
    case 1:
        return &tButtonOne;
    case 2:
        return &tButtonTwo;
    case 3:
        return &tButtonThree;
    default:
        return 0;
    }
}

/**
 * @brief  读取按键 GPIO 电平（multi-button HAL 接口）
 * @param[in] button_id  按键 ID
 * @return GPIO 引脚电平（0 或 1）
 */
static uint8_t UsrButtonReadGpio(uint8_t u8ButtonId)
{
    return BspButtonReadLevel(u8ButtonId);
}

/**
 * @brief  记录按键事件的通用回调
 * @param[in] btn  触发事件的按键结构体指针
 * @note   记录事件到按键历史数组，并打印调试信息。
 */
static void UsrButtonRecordEvent(Button *ptButton)
{
    uint8_t Index;
    ButtonEvent eEvent;

    if ((ptButton == 0) || (ptButton->button_id == 0u) ||
        (ptButton->button_id > USER_BUTTON_COUNT)) {
        return;
    }

    Index = (uint8_t)(ptButton->button_id - 1u);
    eEvent = button_get_event(ptButton);
    au8ButtonLastEvent[Index] = (uint8_t)eEvent;

    SEGGER_RTT_printf(0, "KEY%u event:%u repeat:%u\r\n",
                      ptButton->button_id,
                      (uint8_t)eEvent,
                      button_get_repeat_count(ptButton));
}

static void UsrButtonToggleCallback(Button *ptButton)
{
    UsrButtonRecordEvent(ptButton);
    UsrButtonOutputToggle(ptButton);
}

static void UsrButtonDecreaseCallback(Button *ptButton)
{
    UsrButtonRecordEvent(ptButton);
    UsrButtonValueDec(ptButton);
}

static void UsrButtonIncreaseCallback(Button *ptButton)
{
    UsrButtonRecordEvent(ptButton);
    UsrButtonValueInc(ptButton);
}

/**
 * @brief  初始化所有按键并绑定事件
 * @note   重复调用只执行一次。
 */
void UsrButtonInit(void)
{
    if (u8ButtonInitialized != 0u) {
        return;
    }

    button_init(&tButtonOne, UsrButtonReadGpio, USER_BUTTON_ACTIVE_LEVEL, 1u);
    button_init(&tButtonTwo, UsrButtonReadGpio, USER_BUTTON_ACTIVE_LEVEL, 2u);
    button_init(&tButtonThree, UsrButtonReadGpio, USER_BUTTON_ACTIVE_LEVEL, 3u);

    /* KEY1：长按切换输出开关 VOUT-EN */
    button_attach(&tButtonOne, BTN_LONG_PRESS_START, UsrButtonToggleCallback);
    /* KEY2 / KEY3：单击做数值减 / 加（当前为预留实现） */
    button_attach(&tButtonTwo, BTN_SINGLE_CLICK,    UsrButtonDecreaseCallback);
    button_attach(&tButtonThree, BTN_SINGLE_CLICK,  UsrButtonIncreaseCallback);
    /* 全部按键记录事件到日志，便于调试 */
    button_attach(&tButtonOne, BTN_SINGLE_CLICK,     UsrButtonRecordEvent);
    button_attach(&tButtonTwo, BTN_LONG_PRESS_START, UsrButtonRecordEvent);
    button_attach(&tButtonThree, BTN_LONG_PRESS_START, UsrButtonRecordEvent);

    button_start(&tButtonOne);
    button_start(&tButtonTwo);
    button_start(&tButtonThree);

    u8ButtonInitialized = 1u;
}

/* ===================== 对外按键状态接口 ===================== */

/**
 * @brief Reads the raw level mask for all board buttons.
 * @return Bit 0 through bit 2 contain the GPIO levels of KEY1 through KEY3.
 */
uint8_t UsrButtonGetRawMask(void)
{
    return BspButtonGetRawMask();
}

/**
 * @brief Reports whether a button is in a pressed state.
 * @param[in] u8ButtonId Button identifier from 1 through 3.
 * @retval 1 The button is pressed.
 * @retval 0 The button is released or the identifier is invalid.
 */
uint8_t UsrButtonGetPressed(uint8_t u8ButtonId)
{
    Button *ptButton = UsrButtonGetHandle(u8ButtonId);
    int Pressed;

    if (ptButton == 0) {
        return 0u;
    }

    Pressed = button_is_pressed(ptButton);
    return (Pressed > 0) ? 1u : 0u;
}

/**
 * @brief Builds a mask of the currently pressed buttons.
 * @return Bit 0 through bit 2 indicate KEY1 through KEY3 respectively.
 */
uint8_t UsrButtonGetPressedMask(void)
{
    uint8_t Mask = 0u;

    if (UsrButtonGetPressed(1u) != 0u) Mask |= 0x01u;
    if (UsrButtonGetPressed(2u) != 0u) Mask |= 0x02u;
    if (UsrButtonGetPressed(3u) != 0u) Mask |= 0x04u;

    return Mask;
}

/**
 * @brief Gets the most recently recorded event for a button.
 * @param[in] u8ButtonId Button identifier from 1 through 3.
 * @return A ButtonEvent value, or BTN_NONE_PRESS for an invalid identifier.
 */
uint8_t UsrButtonGetLastEvent(uint8_t u8ButtonId)
{
    if ((u8ButtonId == 0u) || (u8ButtonId > USER_BUTTON_COUNT)) {
        return (uint8_t)BTN_NONE_PRESS;
    }

    return au8ButtonLastEvent[u8ButtonId - 1u];
}

/**
 * @brief  Protothread 按键扫描协程任务
 * @return PT 状态码
 * @note   首次进入时初始化按键，之后以 BUTTON_TIME_MS 为周期调用 button_ticks()
 *         扫描按键状态机。按键库要求周期约 5ms（TICKS_INTERVAL）。
 */
uint16_t UsrButtonTask(void)
{
    PT_BEGIN()
    {
        UsrButtonInit();
    }

    while (1)
    {
        PT_WAIT_UNTIL(BUTTON_TIME_MS / OS_TICK_MS);
        button_ticks();
    }

    PT_END();
}
