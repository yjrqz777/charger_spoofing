/**
 * @file    st7789v.h   (PC 端预览用最小替身)
 * @brief   只保留 ui_font.c / ui_dashboard.c 用到的矩形输出接口，
 *          在 PC 上把像素写进内存帧缓冲，便于导出 PNG 做人工核对。
 */
#ifndef __ST7789V_H__
#define __ST7789V_H__

#include "main.h"

#define LCD_W  240
#define LCD_H  135

void LCD_FillRect(uint16_t u16X, uint16_t u16Y, uint16_t u16W, uint16_t u16H, uint16_t u16Color);
void LCD_BlitRect(uint16_t u16X, uint16_t u16Y, uint16_t u16W, uint16_t u16H,
                  const uint8_t *pu8Rgb565);

#endif /* __ST7789V_H__ */
