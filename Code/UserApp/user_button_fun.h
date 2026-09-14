/**
 * @file    user_button_fun.h
 * @brief   按键功能回调头文件
 *******************************************************************************
 * @note    声明按键事件触发的上层功能函数，供 user_button.c 绑定使用。
 *          与旧版（FOC 项目）相比，已移除全部电机/PID 调参回调。
 *******************************************************************************
 */

#ifndef __USER_BUTTON_FUN_H__
#define __USER_BUTTON_FUN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "Code/user_global.h"
#include "bsp_button.h"

/** @brief 数值调整的步进（预留，供后续可调参数使用） */
#define USER_BUTTON_VALUE_STEP  (1)

/**
 * @brief  KEY1 长按：切换输出开关 VOUT-EN
 * @param[in] ptButton  触发事件的按键结构体指针
 * @note   打开输出前会同步点亮指示灯，关闭输出则熄灭指示灯。
 */
void UsrButtonOutputToggle(Button *ptButton);

/**
 * @brief  KEY2 单击：数值减（预留）
 */
void UsrButtonValueDec(Button *ptButton);

/**
 * @brief  KEY3 单击：数值加（预留）
 */
void UsrButtonValueInc(Button *ptButton);

#ifdef __cplusplus
}
#endif

#endif /* __USER_BUTTON_FUN_H__ */
