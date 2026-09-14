/**
 * @file user_ws2812.h
 * @brief Declares the non-blocking four-pixel WS2812 animation task.
 */

#ifndef __USER_WS2812_H__
#define __USER_WS2812_H__

#include "User_global.h"

#define USR_WS2812_TASK_INTERVAL_MS (10u)  /* Animation update interval. */
#define USR_WS2812_FADE_STEPS       (200u) /* Steps between adjacent colors. */

void UsrWs2812Init(void);
void UsrWs2812Update(void);
uint16_t UsrWs2812Task(void);

#endif /* __USER_WS2812_H__ */
