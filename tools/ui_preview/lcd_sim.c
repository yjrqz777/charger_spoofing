/**
 * @file    lcd_sim.c
 * @brief   PC 预览用的 LCD 替身：把矩形输出画进内存帧缓冲并导出 PPM/PNG
 * @note    仅用于"没有硬件时验证界面布局、基线、字距和颜色"，
 *          不参与固件编译，也不模拟 ST7789V 的窗口/字节序细节
 *          （固件侧的大端字节序由 st7789v.c 的 LCD_BlitRect 负责）。
 */

#include "st7789v/st7789v.h"
#include <stdio.h>
#include <stdlib.h>

static uint16_t s_au16FrameBuffer[LCD_W * LCD_H];
static uint32_t s_u32WriteCount = 0u;   /* 记录矩形写入次数，用于评估 SPI 调用量 */
static uint32_t s_u32PixelCount = 0u;   /* 记录实际写入的像素数 */

void LCD_FillRect(uint16_t u16X, uint16_t u16Y, uint16_t u16W, uint16_t u16H, uint16_t u16Color)
{
    uint16_t x;
    uint16_t y;

    s_u32WriteCount++;
    for (y = u16Y; (y < (uint16_t)(u16Y + u16H)) && (y < LCD_H); y++)
    {
        for (x = u16X; (x < (uint16_t)(u16X + u16W)) && (x < LCD_W); x++)
        {
            s_au16FrameBuffer[(uint32_t)y * LCD_W + x] = u16Color;
            s_u32PixelCount++;
        }
    }
}

void LCD_BlitRect(uint16_t u16X, uint16_t u16Y, uint16_t u16W, uint16_t u16H,
                  const uint8_t *pu8Rgb565)
{
    uint16_t x;
    uint16_t y;

    s_u32WriteCount++;
    for (y = 0u; y < u16H; y++)
    {
        for (x = 0u; x < u16W; x++)
        {
            uint32_t u32Index = ((uint32_t)y * u16W + x) * 2u;
            uint16_t u16Color = (uint16_t)((pu8Rgb565[u32Index] << 8u) | pu8Rgb565[u32Index + 1u]);
            uint16_t u16Px = (uint16_t)(u16X + x);
            uint16_t u16Py = (uint16_t)(u16Y + y);

            if ((u16Px < LCD_W) && (u16Py < LCD_H))
            {
                s_au16FrameBuffer[(uint32_t)u16Py * LCD_W + u16Px] = u16Color;
                s_u32PixelCount++;
            }
        }
    }
}

/** @brief 统计写入次数与像素数，供体积/带宽评估 */
void LcdSim_GetStats(uint32_t *pu32Rects, uint32_t *pu32Pixels)
{
    if (pu32Rects != NULL)
    {
        *pu32Rects = s_u32WriteCount;
    }
    if (pu32Pixels != NULL)
    {
        *pu32Pixels = s_u32PixelCount;
    }
}

/** @brief 复位统计（每帧刷新前调用） */
void LcdSim_ResetStats(void)
{
    s_u32WriteCount = 0u;
    s_u32PixelCount = 0u;
}

/** @brief 导出 PNG（PIL 可读的 PPM 由 Python 端转换，这里先写 PPM） */
int LcdSim_SavePpm(const char *pcPath)
{
    FILE *fp = fopen(pcPath, "wb");
    int y;
    int x;

    if (fp == NULL)
    {
        return -1;
    }

    fprintf(fp, "P6\n%d %d\n255\n", LCD_W, LCD_H);
    for (y = 0; y < LCD_H; y++)
    {
        for (x = 0; x < LCD_W; x++)
        {
            uint16_t c = s_au16FrameBuffer[y * LCD_W + x];
            unsigned char rgb[3];

            /* RGB565 -> RGB888，低位补高位，保证 0xFFFF 仍是纯白 */
            rgb[0] = (unsigned char)(((c >> 11) & 0x1Fu) * 255u / 31u);
            rgb[1] = (unsigned char)(((c >> 5) & 0x3Fu) * 255u / 63u);
            rgb[2] = (unsigned char)((c & 0x1Fu) * 255u / 31u);
            fwrite(rgb, 1u, 3u, fp);
        }
    }

    fclose(fp);
    return 0;
}

/** @brief 取指定像素（用于断言） */
uint16_t LcdSim_GetPixel(uint16_t u16X, uint16_t u16Y)
{
    if ((u16X >= LCD_W) || (u16Y >= LCD_H))
    {
        return 0u;
    }
    return s_au16FrameBuffer[(uint32_t)u16Y * LCD_W + u16X];
}
