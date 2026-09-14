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
#include "bsp_board.h"

void UsrButtonOutputToggle(Button *ptButton)
{
    (void)ptButton;

    if (BspBoardGetVoutEnable() != 0u)
    {
        /* 当前为导通 -> 关断，并熄灭指示灯 */
        BspBoardSetVoutEnable(0u);
        BspBoardSetLed(0u);
        SEGGER_RTT_printf(0, "VOUT-EN -> OFF\r\n");
    }
    else
    {
        /* 当前为关断 -> 导通，并点亮指示灯 */
        BspBoardSetVoutEnable(1u);
        BspBoardSetLed(1u);
        SEGGER_RTT_printf(0, "VOUT-EN -> ON\r\n");
    }
}

void UsrButtonValueDec(Button *ptButton)
{
    (void)ptButton;

    /* 预留：后续如需可调参数（如采样周期、屏幕亮度等）在此实现减操作 */
    SEGGER_RTT_printf(0, "KEY2 value -\r\n");
}

void UsrButtonValueInc(Button *ptButton)
{
    (void)ptButton;

    /* 预留：后续如需可调参数在此实现加操作 */
    SEGGER_RTT_printf(0, "KEY3 value +\r\n");
}
