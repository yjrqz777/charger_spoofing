/**
 * @file user_pd.c
 * @brief Runs the USB Power Delivery sink state machine from the scheduler.
 */

#include "user_pd.h"
#include "bsp_usb_pd.h"

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
    PT_BEGIN()
    {
#if USER_PD_ENABLE
        BspUsbPdInit();
#endif
    }

    while (1)
    {
        PT_WAIT_UNTIL(USR_PD_TASK_INTERVAL_MS / OS_TICK_MS);
#if USER_PD_ENABLE
        BspUsbPdProcess(USR_PD_TASK_INTERVAL_MS);
#endif
    }

    PT_END();
}
