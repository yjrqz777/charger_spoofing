/**
 * @file    user_system.c
 * @brief   系统任务与状态管理实现
 *******************************************************************************
 * @note    状态机（沿用既有接口命名）：
 *            INIT --(上电 100ms)--> POWER_ON --(保持 1s 显示开机页)--> RUNNING
 *            RUNNING 为正常工作状态，持续刷新数据面板。
 *
 *          与旧 FOC 版本的区别：已解除对电机/FOC/Hall/MT6816 模块的依赖。
 *******************************************************************************
 */

#include "user_system.h"
#include "bsp_board.h"
#include "bsp_adc.h"

/** @brief 全局系统数据实例 */
tSysDataDef tSysData;

/** @brief 进入 POWER_ON 后保持开机页的时长（ms），到期转入 RUNNING */
#define USR_SYSTEM_BOOT_SCREEN_MS   (1000u)

/** @brief POWER_ON 已持续时间计数（ms） */
static uint32_t s_u32PowerOnHoldMs = 0u;

void UsrSystemInit(void)
{
    tSysData.eState          = E_SYS_STATE_INIT;
    tSysData.u32PowerOnTimes = 0u;
    tSysData.u32OpenTimes    = 0u;

    s_u32PowerOnHoldMs = 0u;

    /* 上电默认安全状态：输出关断、指示灯熄灭 */
    BspBoardSetVoutEnable(0u);
    BspBoardSetLed(0u);

    /* ADC 采样初始化 */
    BspAdcInit();
    printf("[SYSTEM] initialized, state=INIT\r\n");
}

void UsrSystemUpdate(void)
{
    if (tSysData.eState == E_SYS_STATE_POWER_ON)
    {
        s_u32PowerOnHoldMs += USR_SYSTEM_TASK_INTERVAL_MS;

        if (s_u32PowerOnHoldMs >= USR_SYSTEM_BOOT_SCREEN_MS)
        {
            tSysData.eState    = E_SYS_STATE_RUNNING;
            s_u32PowerOnHoldMs = 0u;
            printf("[SYSTEM] POWER_ON -> RUNNING\r\n");
        }
    }
}

/**
 * @brief  Protothread 系统协程任务
 * @return PT 状态码
 * @note   首次进入执行初始化，之后按 USR_SYSTEM_TASK_INTERVAL_MS 周期推进状态机。
 */
uint16_t UsrSystemTask(void)
{
    PT_BEGIN()
    {
        UsrSystemInit();
    }

    while (1)
    {
        PT_WAIT_UNTIL(USR_SYSTEM_TASK_INTERVAL_MS / OS_TICK_MS);
        UsrSystemUpdate();
    }

    PT_END();
}
