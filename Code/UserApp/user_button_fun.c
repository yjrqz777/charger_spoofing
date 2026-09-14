/**
 * @file    user_button_fun.c
 * @brief   按键功能回调与冻结实现兼容门面
 *******************************************************************************
 * @note    实现按键事件触发的上层功能回调，并为不可修改的
 *          user_button.c 导出真实兼容符号对应的规范接口。
 *******************************************************************************
 */

#include "user_button.h"
#include "bsp_button.h"
#include "user_system.h"
#include "user_foc.h"
#include "user_motor.h"

/** @brief 电流环 Kp 按键调整步进(Q16) */
#define USER_BUTTON_CUR_LOOP_KP_STEP  (2000)
/** @brief 电流环 Ki 按键调整步进(Q16) */
#define USER_BUTTON_CUR_LOOP_KI_STEP  (50)
/** @brief 速度环 Kp 按键调整步进 */
#define USER_BUTTON_SPD_LOOP_KP_STEP  (0.0005f)
/** @brief 速度环 Ki 按键调整步进 */
#define USER_BUTTON_SPD_LOOP_KI_STEP  (0.0005f)

/**
 * @brief  按键状态切换回调函数
 * @param[in] ptButton  触发事件的按键结构体指针
 * @note   绑定到按键1（KEY1）的单击事件（BTN_SINGLE_CLICK）。
 *         在 POWER_ON 和 RUNNING 状态之间切换。
 *         其他状态下不作响应。
 */
void UsrButtonStatusSwitch(Button * ptButton)
{
    (void)ptButton;

    if (tSysData.eState == E_SYS_STATE_POWER_ON)
    {
        tSysData.eState = E_SYS_STATE_RUNNING;
        return;
    }

    if (tSysData.eState == E_SYS_STATE_RUNNING)
    {
        tSysData.eState = E_SYS_STATE_POWER_ON;
        return;
    }
}

/** @brief 冻结 user_button.c 回调所需的真实兼容符号 */
void UserStatusSwitch(Button * ptButton)
{
    UsrButtonStatusSwitch(ptButton);
}

/**
 * @brief  电流环 Kp 增大（KEY1 单击）
 * @note   每次 +USER_BUTTON_CUR_LOOP_KP_STEP(Q16)，同时更新 d/q 轴 PI，
 *         并通过 RTT 输出当前 Kp/Ki 值
 */
void UsrButtonCurrentLoopKpInc(Button * ptButton)
{
    (void)ptButton;

}

/**
 * @brief  电流环 Kp 减小（KEY2 单击）
 */
void UsrButtonCurrentLoopKpDec(Button * ptButton)
{
    (void)ptButton;

}

/**
 * @brief  电流环 Ki 增大（KEY3 单击）
 */
void UsrButtonCurrentLoopKiInc(Button * ptButton)
{
    (void)ptButton;
}

/**
 * @brief  电流环 Ki 减小（KEY4 单击）
 */
void UsrButtonCurrentLoopKiDec(Button * ptButton)
{
    (void)ptButton;
}

/** @brief 冻结 user_button.c 回调所需的真实兼容符号 */
void UserCurrentLoopKpInc(Button * ptButton) { UsrButtonCurrentLoopKpInc(ptButton); }
void UserCurrentLoopKpDec(Button * ptButton) { UsrButtonCurrentLoopKpDec(ptButton); }
void UserCurrentLoopKiInc(Button * ptButton) { UsrButtonCurrentLoopKiInc(ptButton); }
void UserCurrentLoopKiDec(Button * ptButton) { UsrButtonCurrentLoopKiDec(ptButton); }

/* ===================== 速度环 PID 调参 ===================== */

/**
 * @brief  速度环 Kp 增大（KEY1 单击）
 * @note   每次 +USER_BUTTON_SPD_LOOP_KP_STEP，RTT 输出 Kp/Ki（×1e4 整数）
 */
void UsrButtonSpeedLoopKpInc(Button * ptButton)
{
    (void)ptButton;
}

/**
 * @brief  速度环 Kp 减小（KEY2 单击）
 */
void UsrButtonSpeedLoopKpDec(Button * ptButton)
{
    (void)ptButton;
}

/**
 * @brief  速度环 Ki 增大（KEY3 单击）
 */
void UsrButtonSpeedLoopKiInc(Button * ptButton)
{
    (void)ptButton;
}

/**
 * @brief  速度环 Ki 减小（KEY4 单击）
 */
void UsrButtonSpeedLoopKiDec(Button * ptButton)
{
    (void)ptButton;
}

/** @brief 冻结 user_button.c 回调所需的真实兼容符号 */
void UserSpeedLoopKpInc(Button * ptButton) { UsrButtonSpeedLoopKpInc(ptButton); }
void UserSpeedLoopKpDec(Button * ptButton) { UsrButtonSpeedLoopKpDec(ptButton); }
void UserSpeedLoopKiInc(Button * ptButton) { UsrButtonSpeedLoopKiInc(ptButton); }
void UserSpeedLoopKiDec(Button * ptButton) { UsrButtonSpeedLoopKiDec(ptButton); }

void UsrButtonInit(void)
{
    buttons_init();
}

uint8_t UsrButtonGetRawMask(void)
{
    return UserButton_GetRawMask();
}

uint8_t UsrButtonGetPressed(uint8_t u8ButtonId)
{
    return UserButton_GetPressed(u8ButtonId);
}

uint8_t UsrButtonGetPressedMask(void)
{
    return UserButton_GetPressedMask();
}

uint8_t UsrButtonGetLastEvent(uint8_t u8ButtonId)
{
    return UserButton_GetLastEvent(u8ButtonId);
}

uint16_t UsrButtonTask(void)
{
    return PtTaskButton();
}
