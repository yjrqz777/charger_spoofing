/**
 * @file    user_display.h
 * @brief   用户显示任务头文件 — Protothread 协程驱动 LCD 刷新
 *******************************************************************************
 * @note    每 10ms 调度一次显示任务，按现有状态逻辑刷新 LCD 和 WS2812。
 *******************************************************************************
 */

#ifndef __USER_DISPLAY_H__
#define __USER_DISPLAY_H__
#include "Code/user_global.h"
#include "user_system.h"

#define USR_DISPLAY_TASK_INTERVAL_MS (10u)  /**< 显示刷新周期（毫秒） */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 显示状态-函数映射表项（将系统状态映射到对应的显示处理函数） */
typedef struct tDisplayFunctionDataDef
{
    eSysStateDef eState;         /**< 系统状态 */
    void (* pfFunction)(void);   /**< 对应的显示处理函数指针 */
} tDisplayFunctionDataDef;

/** @brief 显示数据全局结构体 */
typedef struct tDisplayDataDef
{
    uint8_t u8Color[3];          /**< WS2812 RGB 颜色值，索引 [R, G, B] */
    uint16_t u16Rgb;             /**< 备用 RGB 颜色值 */
} tDisplayDataDef;

extern tDisplayDataDef tDisplayData;

void UsrDisplayInit(void);
uint16_t UsrDisplayTask(void);

#ifdef __cplusplus
}
#endif

#endif /* __USER_DISPLAY_H__ */
