/**
 * @file    bsp_lcd.h
 * @brief   LCD 显示底层驱动头文件
 *******************************************************************************
 * @note    封装 ST7789V 驱动，提供应用层便捷显示接口
 *******************************************************************************
 */

#ifndef __BSP_LCD_H__
#define __BSP_LCD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "user_global.h"

void BspLcdInit(void);
HAL_StatusTypeDef BspLcdShowUInt(uint16_t u16X, uint16_t u16Y, uint32_t u32Value, uint8_t u8Length);
HAL_StatusTypeDef BspLcdShowUIntColor(uint16_t u16X, uint16_t u16Y, uint32_t u32Value,
                                      uint8_t u8Length, uint16_t u16Color);
HAL_StatusTypeDef BspLcdShowFloatColor(uint16_t u16X, uint16_t u16Y, float f32Value,
                                       uint8_t u8Length, uint16_t u16Color);

/* 字段缓冲式刷新：应用层收集待显示字段，BSP 层按 DMA 就绪节奏逐字段输出 */
void BspLcdBeginRefresh(uint8_t u8StateId);
void BspLcdAddUInt(uint16_t u16X, uint16_t u16Y, uint32_t u32Value,
                   uint8_t u8Length, uint16_t u16Color);
void BspLcdAddFloat(uint16_t u16X, uint16_t u16Y, float f32Value,
                    uint8_t u8Length, uint16_t u16Color);
void BspLcdService(uint8_t u8StateId);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LCD_H__ */
