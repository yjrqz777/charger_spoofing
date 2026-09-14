/**
 * @file user_pd.h
 * @brief Declares the USB Power Delivery sink task.
 */

#ifndef __USER_PD_H__
#define __USER_PD_H__

#include "User_global.h"

#define USR_PD_TASK_INTERVAL_MS (1u) /* PD state-machine service interval. */

uint16_t UsrPdTask(void);
void UsrPdSelectPreviousPdo(void);
void UsrPdSelectNextPdo(void);

#endif /* __USER_PD_H__ */
