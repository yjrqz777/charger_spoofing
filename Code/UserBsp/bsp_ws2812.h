/**
 * @file bsp_ws2812.h
 * @brief Declares the four-pixel WS2812 PWM and DMA driver.
 */

#ifndef __BSP_WS2812_H__
#define __BSP_WS2812_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define BSP_WS2812_LED_COUNT        (4u)  /* Number of cascaded pixels. */
#define BSP_WS2812_BITS_PER_PIXEL   (24u) /* WS2812 GRB payload width. */
#define BSP_WS2812_RESET_SLOTS      (60u) /* 75 us low reset interval. */
#define BSP_WS2812_PWM_PERIOD       (10u) /* 1.25 us at an 8 MHz timer clock. */
#define BSP_WS2812_PWM_ZERO_COMPARE (3u)  /* Logical zero high time: 0.375 us. */
#define BSP_WS2812_PWM_ONE_COMPARE  (7u)  /* Logical one high time: 0.875 us. */
#define BSP_WS2812_BUFFER_LENGTH    ((BSP_WS2812_LED_COUNT * BSP_WS2812_BITS_PER_PIXEL) + \
                                     BSP_WS2812_RESET_SLOTS)

void BspWs2812Init(void);
eStatusDef BspWs2812SetPixel(uint16_t u16Index, uint8_t u8Red,
                             uint8_t u8Green, uint8_t u8Blue);
eStatusDef BspWs2812Fill(uint8_t u8Red, uint8_t u8Green, uint8_t u8Blue);
eStatusDef BspWs2812Show(void);
uint8_t BspWs2812IsIdle(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_WS2812_H__ */
