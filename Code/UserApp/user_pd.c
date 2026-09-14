/**
 * @file user_pd.c
 * @brief Runs the USB Power Delivery sink state machine from the scheduler.
 */

#include "user_pd.h"
#include "bsp_usb_pd.h"
#include "bsp_lcd.h"      /* BspLcdGetDroppedOps()：[RUN] 诊断用 */

/**
 * @brief Requests the fixed PDO immediately below the current selection.
 */
void UsrPdSelectPreviousPdo(void)
{
    const tBspUsbPdStatusDef *ptPdStatus;
    uint8_t RequestedPdo;
    eStatusDef eResult;

    ptPdStatus = BspUsbPdGetStatus();
    if ((ptPdStatus->u8ContractValid == 0u) ||
        (ptPdStatus->u8RequestedPdo == 0u))
    {
        printf("[PD] voltage down unavailable: no active contract\r\n");
        return;
    }

    if (ptPdStatus->u8RequestedPdo <= 1u)
    {
        printf("[PD] already at lowest PDO\r\n");
        return;
    }

    RequestedPdo = (uint8_t)(ptPdStatus->u8RequestedPdo - 1u);
    eResult = BspUsbPdRequestPdo(RequestedPdo);
    if (eResult == E_BUSY)
    {
        printf("[PD] voltage down ignored: negotiation busy\r\n");
    }
    else if (eResult != E_OK)
    {
        printf("[PD] voltage down failed\r\n");
    }
}

/**
 * @brief Requests the fixed PDO immediately above the current selection.
 */
void UsrPdSelectNextPdo(void)
{
    const tBspUsbPdStatusDef *ptPdStatus;
    uint8_t RequestedPdo;
    eStatusDef eResult;

    ptPdStatus = BspUsbPdGetStatus();
    if ((ptPdStatus->u8ContractValid == 0u) ||
        (ptPdStatus->u8RequestedPdo == 0u))
    {
        printf("[PD] voltage up unavailable: no active contract\r\n");
        return;
    }

    if (ptPdStatus->u8RequestedPdo >= ptPdStatus->u8PdoCount)
    {
        printf("[PD] already at highest fixed PDO\r\n");
        return;
    }

    RequestedPdo = (uint8_t)(ptPdStatus->u8RequestedPdo + 1u);
    eResult = BspUsbPdRequestPdo(RequestedPdo);
    if (eResult == E_BUSY)
    {
        printf("[PD] voltage up ignored: negotiation busy\r\n");
    }
    else if (eResult != E_OK)
    {
        printf("[PD] voltage up failed\r\n");
    }
}

/**
 * @brief Services USB Power Delivery negotiation once per millisecond.
 * @return Scheduler delay in system ticks.
 */
uint16_t UsrPdTask(void)
{
    uint16_t u16DiagAcc = 0u;
    uint16_t u16LoopCount = 0u;
    uint16_t u16LoopsLastSecond = 0u;

    PT_BEGIN()
    {
#if USER_PD_ENABLE
        BspUsbPdInit();
#endif
    }

    /* 例程在 PD_Init() 之后先建立 TIM1 时基、再进主循环，第一次调用
     * PD_Main_Proc() 时硬件已经稳定。这里同样留出 20ms 让 USBPD PHY 与
     * CC 比较器稳定后再开始检测。 */
    PT_WAIT_UNTIL(20u / OS_TICK_MS);

    while (1)
    {
        PT_WAIT_UNTIL(USR_PD_TASK_INTERVAL_MS / OS_TICK_MS);
#if USER_PD_ENABLE
        BspUsbPdProcess(USR_PD_TASK_INTERVAL_MS);
#endif

        /* [RUN] 由本任务代替显示任务输出：每秒一条，并打印本秒内 PD 任务
         * 实际被调度了多少次（理论 1000 次）。这个数字直接反映主循环是否
         * 被别的耗时操作拖慢 —— LCD 整屏填充曾把它压到每秒十几次。 */
        u16LoopCount++;
        u16DiagAcc++;
        if (u16DiagAcc >= 1000u)
        {
            const tBspUsbPdStatusDef *ptPdStatus = BspUsbPdGetStatus();

            printf("[RUN] PD=%u/%u %umV %umA loops=%u/s cc=%u lcd_drop=%u\r\n",
                   (unsigned int)ptPdStatus->u8RequestedPdo,
                   (unsigned int)ptPdStatus->u8PdoCount,
                   (unsigned int)ptPdStatus->u16VoltageMv,
                   (unsigned int)ptPdStatus->u16CurrentMa,
                   (unsigned int)u16LoopsLastSecond,
                   (unsigned int)ptPdStatus->u8CcLine,
                   (unsigned int)BspLcdGetDroppedOps());

            u16LoopsLastSecond = u16LoopCount;
            u16LoopCount = 0u;
            u16DiagAcc = 0u;
        }
    }

    PT_END();
}
