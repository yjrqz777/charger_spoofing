/**
 * @file    user_display.h
 * @brief   用户显示任务头文件 — Protothread 协程驱动 LCD 刷新
 *******************************************************************************
 * @note    每 10ms 调度一次显示任务，按系统状态刷新 LCD。
 *          屏幕为 240x135 横屏（ST7789V），RUNNING 页由 Code/UserApp/ui/
 *          的仪表界面负责，显示：VBUS / VOUT / IBUS / 输入功率 / 输出开关状态。
 *******************************************************************************
 */

#ifndef __USER_DISPLAY_H__
#define __USER_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "User_global.h"
#include "user_system.h"

/** @brief 显示任务调度周期（毫秒） */
#define USR_DISPLAY_TASK_INTERVAL_MS (10u)

/** @brief 数据面板的刷新间隔（毫秒），5Hz */
#define USR_DISPLAY_REFRESH_MS       (200u)

/** @brief 采样更新间隔（毫秒） */
#define USR_DISPLAY_SAMPLE_MS        (100u)

void UsrDisplayInit(void);
uint16_t UsrDisplayTask(void);

#ifdef __cplusplus
}
#endif

#endif /* __USER_DISPLAY_H__ */
