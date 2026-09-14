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

/** @brief 本板按键总数（原理图 SW1/SW2/SW3） */
#define USER_BUTTON_NUM          3u
/** @brief 按键按下时的有效电平（低电平有效） */
#define USER_BUTTON_ACTIVE_LEVEL 0u

/** @brief 3 个按键的 Button 结构体实例 */
static Button btn1;
static Button btn2;
static Button btn3;

/** @brief 初始化完成标志 */
static uint8_t button_inited = 0u;

/** @brief 各按键上次触发事件记录 */
static uint8_t button_last_event[USER_BUTTON_NUM] = {0u};

/**
 * @brief  根据 ID 获取按键结构体指针
 * @param[in] button_id  按键 ID（1~3）
 * @return Button 结构体指针，无效 ID 返回 NULL
 */
static Button *UserButton_GetHandle(uint8_t button_id)
{
    switch (button_id) {
    case 1:
        return &btn1;
    case 2:
        return &btn2;
    case 3:
        return &btn3;
    default:
        return 0;
    }
}

/**
 * @brief  读取按键 GPIO 电平（multi-button HAL 接口）
 * @param[in] button_id  按键 ID
 * @return GPIO 引脚电平（0 或 1）
 */
static uint8_t read_button_gpio(uint8_t button_id)
{
    return BspButton_ReadLevel(button_id);
}

/**
 * @brief  记录按键事件的通用回调
 * @param[in] btn  触发事件的按键结构体指针
 * @note   记录事件到 button_last_event 数组，并打印调试信息。
 */
static void button_event_handler(Button *btn)
{
    uint8_t index;
    ButtonEvent event;

    if ((btn == 0) || (btn->button_id == 0u) || (btn->button_id > USER_BUTTON_NUM)) {
        return;
    }

    index = (uint8_t)(btn->button_id - 1u);
    event = button_get_event(btn);
    button_last_event[index] = (uint8_t)event;

    SEGGER_RTT_printf(0, "KEY%u event:%u repeat:%u\r\n",
                      btn->button_id,
                      (uint8_t)event,
                      button_get_repeat_count(btn));
}

/**
 * @brief  初始化所有按键并绑定事件
 * @note   重复调用只执行一次。
 */
void buttons_init(void)
{
    if (button_inited != 0u) {
        return;
    }

    button_init(&btn1, read_button_gpio, USER_BUTTON_ACTIVE_LEVEL, 1u);
    button_init(&btn2, read_button_gpio, USER_BUTTON_ACTIVE_LEVEL, 2u);
    button_init(&btn3, read_button_gpio, USER_BUTTON_ACTIVE_LEVEL, 3u);

    /* KEY1：长按切换输出开关 VOUT-EN */
    button_attach(&btn1, BTN_LONG_PRESS_START, UsrButtonOutputToggle);
    /* KEY2 / KEY3：单击做数值减 / 加（当前为预留实现） */
    button_attach(&btn2, BTN_SINGLE_CLICK,    UsrButtonValueDec);
    button_attach(&btn3, BTN_SINGLE_CLICK,    UsrButtonValueInc);
    /* 全部按键记录事件到日志，便于调试 */
    button_attach(&btn1, BTN_SINGLE_CLICK,    button_event_handler);
    button_attach(&btn2, BTN_LONG_PRESS_START, button_event_handler);
    button_attach(&btn3, BTN_LONG_PRESS_START, button_event_handler);

    button_start(&btn1);
    button_start(&btn2);
    button_start(&btn3);

    button_inited = 1u;
}

/* ===================== 冻结实现使用的兼容符号 ===================== */

uint8_t UserButton_GetRawMask(void)
{
    return BspButton_GetRawMask();
}

uint8_t UserButton_GetPressed(uint8_t button_id)
{
    Button *btn = UserButton_GetHandle(button_id);
    int pressed;

    if (btn == 0) {
        return 0u;
    }

    pressed = button_is_pressed(btn);
    return (pressed > 0) ? 1u : 0u;
}

uint8_t UserButton_GetPressedMask(void)
{
    uint8_t mask = 0u;

    if (UserButton_GetPressed(1u) != 0u) mask |= 0x01u;
    if (UserButton_GetPressed(2u) != 0u) mask |= 0x02u;
    if (UserButton_GetPressed(3u) != 0u) mask |= 0x04u;

    return mask;
}

uint8_t UserButton_GetLastEvent(uint8_t button_id)
{
    if ((button_id == 0u) || (button_id > USER_BUTTON_NUM)) {
        return (uint8_t)BTN_NONE_PRESS;
    }

    return button_last_event[button_id - 1u];
}

/**
 * @brief  Protothread 按键扫描协程任务
 * @return PT 状态码
 * @note   首次进入时初始化按键，之后以 BUTTON_TIME_MS 为周期调用 button_ticks()
 *         扫描按键状态机。按键库要求周期约 5ms（TICKS_INTERVAL）。
 */
uint16_t PtTaskButton(void)
{
    PT_BEGIN()
    {
        buttons_init();
    }

    while (1)
    {
        PT_WAIT_UNTIL(BUTTON_TIME_MS / OS_TICK_MS);
        button_ticks();
    }

    PT_END();
}
