/**
 * @file    user_button_fun.c
 * @brief   按键功能回调实现
 *******************************************************************************
 * @note    实现按键事件触发的上层功能。
 *          与旧版（FOC 项目）相比，已移除全部电机/PID 调参回调，
 *          改为本项目实际需要的：输出开关切换 + 预留的数值加减。
 *******************************************************************************
 */

#include "user_button_fun.h"
#include "user_system.h"
#include "user_pd.h"
#include "bsp_board.h"

/**
 * @brief Toggles the switched VOUT path and its status indicator.
 * @param[in] ptButton Pointer to the button that triggered the action.
 */
void UsrButtonOutputToggle(Button *ptButton)
{
    (void)ptButton;

    if (BspBoardGetVoutEnable() != 0u)
    {
        /* 当前为导通 -> 关断，并熄灭指示灯 */
        BspBoardSetVoutEnable(0u);
        BspBoardSetLed(0u);
        printf("VOUT-EN -> OFF\r\n");
    }
    else
    {
        /* 当前为关断 -> 导通，并点亮指示灯 */
        BspBoardSetVoutEnable(1u);
        BspBoardSetLed(1u);
        printf("VOUT-EN -> ON\r\n");
    }
}

/**
 * @brief Requests the next lower fixed USB-PD voltage profile.
 * @param[in] ptButton Pointer to the button that triggered the action.
 */
void UsrButtonValueDec(Button *ptButton)
{
    (void)ptButton;
    UsrPdSelectPreviousPdo();
}

/**
 * @brief Requests the next higher fixed USB-PD voltage profile.
 * @param[in] ptButton Pointer to the button that triggered the action.
 */
void UsrButtonValueInc(Button *ptButton)
{
    (void)ptButton;
    UsrPdSelectNextPdo();
}
