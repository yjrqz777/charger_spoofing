/**
 * @file    user_button.c
 * @brief   用户按键管理实现
 *******************************************************************************
 * @note    基于 multi-button 库的 4 按键管理。
 *          支持单击、双击、长按、重复触发等事件，
 *          按键事件通过 RTT 输出调试信息。
 *******************************************************************************
 */

#include "user_button.h"
#include "bsp_button.h"

#define USER_BUTTON_NUM          4u   /**< 按键总数 */
#define USER_BUTTON_ACTIVE_LEVEL 0u   /**< 按键按下时的有效电平（低电平有效） */

extern void UserStatusSwitch(Button *btn);
extern void UserCurrentLoopKpInc(Button *btn);
extern void UserCurrentLoopKpDec(Button *btn);
extern void UserCurrentLoopKiInc(Button *btn);
extern void UserCurrentLoopKiDec(Button *btn);
extern void UserSpeedLoopKpInc(Button *btn);
extern void UserSpeedLoopKpDec(Button *btn);
extern void UserSpeedLoopKiInc(Button *btn);
extern void UserSpeedLoopKiDec(Button *btn);


/** @brief 4 个按键的 Button 结构体实例 */
static Button btn1;
static Button btn2;
static Button btn3;
static Button btn4;

/** @brief 初始化完成标志 */
static uint8_t button_inited = 0u;

/** @brief 各按键上次触发事件记录 */
static uint8_t button_last_event[USER_BUTTON_NUM] = {0u};

/**
 * @brief  根据 ID 获取按键结构体指针
 * @param[in] button_id  按键 ID（1~4）
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
    case 4:
        return &btn4;
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
 * @brief  按键事件通用回调函数
 * @param[in] btn  触发事件的按键结构体指针
 * @note   记录事件到 button_last_event 数组，并通过 RTT 输出日志
 */
// static void button_event_handler(Button *btn)
// {
//     uint8_t index;
//     ButtonEvent event;

//     if (btn == 0 || btn->button_id == 0u || btn->button_id > USER_BUTTON_NUM) {
//         return;
//     }

//     index = (uint8_t)(btn->button_id - 1u);
//     event = button_get_event(btn);
//     button_last_event[index] = (uint8_t)event;

//     SEGGER_RTT_printf(0, "KEY%u event:%u repeat:%u\r\n",
//                       btn->button_id,
//                       (uint8_t)event,
//                       button_get_repeat_count(btn));
// }

/**
 * @brief  为指定按键注册所有事件回调
 * @param[in] btn  按键结构体指针
 * @note   注册事件：PRESS_DOWN, PRESS_UP, PRESS_REPEAT,
 *         SINGLE_CLICK, DOUBLE_CLICK, LONG_PRESS_START, LONG_PRESS_HOLD
 */
// static void button_attach_all_events(Button *btn)
// {
    // button_attach(btn, BTN_PRESS_DOWN, button_event_handler);
    // button_attach(btn, BTN_PRESS_UP, button_event_handler);
    // button_attach(btn, BTN_PRESS_REPEAT, button_event_handler);
    // button_attach(btn, BTN_SINGLE_CLICK, button_event_handler);
    // button_attach(btn, BTN_DOUBLE_CLICK, button_event_handler);
    // button_attach(btn, BTN_LONG_PRESS_START, button_event_handler);
    // button_attach(btn, BTN_LONG_PRESS_HOLD, button_event_handler);
// }

/**
 * @brief  初始化所有 4 个按键
 * @note   依次初始化 btn1~btn4，注册所有事件回调，
 *         并将按键加入轮询链表。重复调用只执行一次。
 */
void buttons_init(void)
{
    if (button_inited != 0u) {
        return;
    }

    button_init(&btn1, read_button_gpio, USER_BUTTON_ACTIVE_LEVEL, 1u);
    button_init(&btn2, read_button_gpio, USER_BUTTON_ACTIVE_LEVEL, 2u);
    button_init(&btn3, read_button_gpio, USER_BUTTON_ACTIVE_LEVEL, 3u);
    button_init(&btn4, read_button_gpio, USER_BUTTON_ACTIVE_LEVEL, 4u);

    // button_attach_all_events(&btn1);
    button_attach(&btn1, BTN_LONG_PRESS_START, UserStatusSwitch);

    /* KEY1~KEY4 单击：速度环 PID 调参 */
    button_attach(&btn1, BTN_SINGLE_CLICK, UserSpeedLoopKpInc);
    button_attach(&btn2, BTN_SINGLE_CLICK, UserSpeedLoopKpDec);
    button_attach(&btn3, BTN_SINGLE_CLICK, UserSpeedLoopKiInc);
    button_attach(&btn4, BTN_SINGLE_CLICK, UserSpeedLoopKiDec);
    // button_attach_all_events(&btn2);
    // button_attach_all_events(&btn3);
    // button_attach_all_events(&btn4);

    button_start(&btn1);
    button_start(&btn2);
    button_start(&btn3);
    button_start(&btn4);

    button_inited = 1u;
}

/**
 * @brief  获取按键 GPIO 原始电平掩码
 * @return 4-bit 掩码，bit0~bit3 对应 KEY1~KEY4
 * @note   直接读取 GPIO 电平，不受去抖逻辑影响
 */
uint8_t UserButton_GetRawMask(void)
{
    return BspButton_GetRawMask();
}

/**
 * @brief  查询指定按键当前按下状态
 * @param[in] button_id  按键 ID（1~4）
 * @retval 1  正在按下
 * @retval 0  未按下或 ID 无效
 */
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

/**
 * @brief  获取所有按键的按下状态掩码
 * @return 4-bit 掩码，bit0~bit3 对应 KEY1~KEY4
 */
uint8_t UserButton_GetPressedMask(void)
{
    uint8_t mask = 0u;

    if (UserButton_GetPressed(1u) != 0u) mask |= 0x01u;
    if (UserButton_GetPressed(2u) != 0u) mask |= 0x02u;
    if (UserButton_GetPressed(3u) != 0u) mask |= 0x04u;
    if (UserButton_GetPressed(4u) != 0u) mask |= 0x08u;

    return mask;
}

/**
 * @brief  获取指定按键的上次触发事件
 * @param[in] button_id  按键 ID（1~4）
 * @return 事件类型（ButtonEvent 枚举值）
 * @retval BTN_NONE_PRESS  ID 无效或未触发事件
 */
uint8_t UserButton_GetLastEvent(uint8_t button_id)
{
    if (button_id == 0u || button_id > USER_BUTTON_NUM) {
        return (uint8_t)BTN_NONE_PRESS;
    }

    return button_last_event[button_id - 1u];
}

/**
 * @brief  Protothread 按键扫描协程任务
 * @return PT 状态码
 * @note   首次进入时初始化按键，
 *         之后以 BUTTON_TIME_MS / OS_TICK_MS 为周期调用 button_ticks() 扫描按键
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
        // button_ticks();
    }

    PT_END();
}
