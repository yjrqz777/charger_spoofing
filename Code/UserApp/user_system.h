/**
 * @file    user_system.h
 * @brief   系统任务与状态管理头文件
 *******************************************************************************
 * @note    定义系统状态枚举、系统数据结构体和系统任务接口。
 *          系统状态机：INIT -> POWER_ON -> RUNNING -> OFF
 *******************************************************************************
 */

#ifndef __USER_SYSTEM_H__
#define __USER_SYSTEM_H__

#include "User_global.h"

/** @brief 系统任务调度周期（毫秒） */
#define USR_SYSTEM_TASK_INTERVAL_MS (5u)

/**
 * @brief 系统状态枚举
 * @note  状态切换：INIT -> POWER_ON 由 user_time.c 按上电时间触发；
 *        POWER_ON -> RUNNING 由 user_system.c 的 UsrSystemUpdate() 触发。
 */
typedef enum
{
    E_SYS_STATE_INIT = 0,       /**< 初始状态，启动后默认进入 */
    E_SYS_STATE_POWER_ON,       /**< 上电状态：显示开机页，100ms 后从 INIT 自动切换 */
    E_SYS_STATE_OFF,            /**< 关闭状态（预留） */
    E_SYS_STATE_RUNNING,        /**< 运行状态：显示实时数据面板 */
    E_SYS_STATE_MAX             /**< 状态总数（边界标记） */
} eSysStateDef;

/** @brief 系统数据全局结构体 */
typedef struct tSysDataDef
{
    eSysStateDef eState;           /**< 当前系统状态 */
    uint32_t u32PowerOnTimes;      /**< 上电时间计数（单位：ms），从 INIT 开始累计 */
    uint32_t u32OpenTimes;         /**< 开机时间计数（单位：ms），进入 RUNNING 后累计 */
} tSysDataDef;

/** @brief 全局系统数据实例 */
extern tSysDataDef tSysData;

void UsrSystemInit(void);
void UsrSystemUpdate(void);
uint16_t UsrSystemTask(void);

#endif /* __USER_SYSTEM_H__ */
