/**
 * @file    user_time.h
 * @brief   系统时间管理任务头文件
 *******************************************************************************
 * @note    提供系统计时功能，包括上电计时和开机计时。
 *          USR_TIME_TASK_INTERVAL_MS 决定时间任务的调度周期。
 *******************************************************************************
 */

#ifndef __USER_TIME_H__
#define __USER_TIME_H__

#include "Code/user_global.h"

/** @brief 时间任务调度周期（毫秒） */
#define USR_TIME_TASK_INTERVAL_MS (10u)

void UsrTimeInit(void);
void UsrTimeUpdate(void);
uint16_t UsrTimeTask(void);

#endif /* __USER_TIME_H__ */
