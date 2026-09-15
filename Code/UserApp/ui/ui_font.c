/**
 * @file    ui_font.c
 * @brief   轻量 2bpp 点阵文字渲染器实现
 *******************************************************************************
 * @note    渲染流程（每个字形一次 SPI 传输）：
 *            1) 在字库的字形表里按码点找到 UiGlyph；
 *            2) 逐像素取出 2bpp 覆盖度，用 4 项颜色 LUT 混合前景/背景色，
 *               结果写入模块内的字形暂存缓冲（RGB565 大端字节序）；
 *            3) 用 LCD_BlitRect() 把该缓冲输出到"单个字形矩形"。
 *
 *          内存策略：
 *            - 没有整屏 RGB565 帧缓冲，也没有动态分配；
 *            - 只有一个 20x24 像素的字形暂存缓冲（960 字节），
 *              当前最大字形为 24px 字号的 16x17，留有余量；
 *            - 超出暂存缓冲或超出屏幕边界的字形会被整体跳过，
 *              不做半字形裁剪，避免布局出错时"偷偷"显示成半截字。
 *******************************************************************************
 */

#include "ui_font.h"
#include "st7789v/st7789v.h"

/** @brief 字形暂存缓冲支持的最大宽度（像素） */
#define UI_FONT_GLYPH_MAX_W   (20u)
/** @brief 字形暂存缓冲支持的最大高度（像素） */
#define UI_FONT_GLYPH_MAX_H   (24u)

/** @brief 字形暂存缓冲（RGB565，每像素 2 字节） */
static uint8_t s_au8GlyphBuffer[UI_FONT_GLYPH_MAX_W * UI_FONT_GLYPH_MAX_H * 2u];

/**
 * @brief  查询字符对应的字形
 * @param[in] ptFont  字库
 * @param[in] u8Char  字符（ASCII 码点）
 * @return 字形描述指针；字符不在字库中时返回 0
 * @note   字形表按字符子集生成（最大 40 项），线性扫描的代价远小于一次 SPI 传输。
 */
const UiGlyph *UiFont_FindGlyph(const UiFont *ptFont, uint8_t u8Char)
{
    uint16_t u16Index;

    if (ptFont == 0)
    {
        return 0;
    }

    for (u16Index = 0u; u16Index < ptFont->glyph_count; u16Index++)
    {
        if (ptFont->glyphs[u16Index].codepoint == (uint32_t)u8Char)
        {
            return &ptFont->glyphs[u16Index];
        }
    }

    return 0;
}

/**
 * @brief  RGB565 前景/背景按覆盖度混合
 * @param[in] u16Fg    前景色
 * @param[in] u16Bg    背景色
 * @param[in] u8Alpha  覆盖度 0..3
 * @return 混合后的 RGB565 颜色
 * @note   覆盖度为 0/3 时直接取纯色，避免整数除法的舍入误差让纯色变脏。
 */
uint16_t UiFont_Blend565(uint16_t u16Fg, uint16_t u16Bg, uint8_t u8Alpha)
{
    uint32_t u32R;
    uint32_t u32G;
    uint32_t u32B;

    if (u8Alpha == 0u)
    {
        return u16Bg;
    }
    if (u8Alpha >= 3u)
    {
        return u16Fg;
    }

    /* 按 R(5) / G(6) / B(5) 分量分别做 a:（3-a）加权平均 */
    u32R = ((((u16Fg >> 11) & 0x1Fu) * u8Alpha) +
            (((u16Bg >> 11) & 0x1Fu) * (3u - u8Alpha))) / 3u;
    u32G = ((((u16Fg >> 5) & 0x3Fu) * u8Alpha) +
            (((u16Bg >> 5) & 0x3Fu) * (3u - u8Alpha))) / 3u;
    u32B = (((u16Fg & 0x1Fu) * u8Alpha) +
            ((u16Bg & 0x1Fu) * (3u - u8Alpha))) / 3u;

    return (uint16_t)((u32R << 11) | (u32G << 5) | u32B);
}

/**
 * @brief  绘制单个字形
 * @param[in] i32PenX        笔位置横坐标
 * @param[in] i16BaselineY   基线纵坐标
 * @param[in] ptFont         字库
 * @param[in] ptGlyph        字形
 * @param[in] u16Fg          前景色
 * @param[in] u16Bg          背景色
 * @note   字形矩形必须完整落在屏幕内，否则整体跳过（含缓冲区放不下的情况）。
 */
static void UiFont_DrawGlyph(int32_t i32PenX, int16_t i16BaselineY, const UiFont *ptFont,
                             const UiGlyph *ptGlyph, uint16_t u16Fg, uint16_t u16Bg)
{
    const uint8_t *pu8Bits;
    uint16_t u16BytesPerRow;
    uint16_t u16Row;
    uint16_t u16Col;
    uint32_t u32Out;
    int32_t  i32X;
    int32_t  i32Y;
    uint16_t au16ColorLut[4];

    if ((ptGlyph->width == 0u) || (ptGlyph->height == 0u))
    {
        return;   /* 空格等无墨迹字符 */
    }
    if ((ptGlyph->width > UI_FONT_GLYPH_MAX_W) || (ptGlyph->height > UI_FONT_GLYPH_MAX_H))
    {
        return;   /* 暂存缓冲放不下，整体跳过 */
    }

    i32X = i32PenX + (int32_t)ptGlyph->x_offset;
    i32Y = (int32_t)i16BaselineY + (int32_t)ptGlyph->y_offset;

    if ((i32X < 0) || (i32Y < 0) ||
        ((i32X + (int32_t)ptGlyph->width) > (int32_t)LCD_W) ||
        ((i32Y + (int32_t)ptGlyph->height) > (int32_t)LCD_H))
    {
        return;   /* 越界字形不画，交给布局去修，不做半截裁剪 */
    }

    /* 4 项颜色 LUT：把逐像素的混合运算收敛成查表 */
    au16ColorLut[0] = u16Bg;
    au16ColorLut[1] = UiFont_Blend565(u16Fg, u16Bg, 1u);
    au16ColorLut[2] = UiFont_Blend565(u16Fg, u16Bg, 2u);
    au16ColorLut[3] = u16Fg;

    pu8Bits = &ptFont->bitmap[ptGlyph->bitmap_offset];
    u16BytesPerRow = (uint16_t)((ptGlyph->width + 3u) / 4u);

    u32Out = 0u;
    for (u16Row = 0u; u16Row < ptGlyph->height; u16Row++)
    {
        for (u16Col = 0u; u16Col < ptGlyph->width; u16Col++)
        {
            uint8_t  u8Alpha;
            uint16_t u16Color;

            u8Alpha = (uint8_t)((pu8Bits[(uint32_t)u16Row * u16BytesPerRow + (u16Col >> 2)] >>
                                 (6u - 2u * (u16Col & 0x03u))) & 0x03u);
            u16Color = au16ColorLut[u8Alpha];

            s_au8GlyphBuffer[u32Out] = (uint8_t)(u16Color >> 8u);
            u32Out++;
            s_au8GlyphBuffer[u32Out] = (uint8_t)u16Color;
            u32Out++;
        }
    }

    LCD_BlitRect((uint16_t)i32X, (uint16_t)i32Y, ptGlyph->width, ptGlyph->height,
                 s_au8GlyphBuffer);
}

void UiFont_DrawText(int16_t i16X, int16_t i16BaselineY, const UiFont *ptFont,
                     uint16_t u16Fg, uint16_t u16Bg, const char *pcText)
{
    int32_t  i32PenX;
    uint16_t u16Index = 0u;

    if ((ptFont == 0) || (pcText == 0))
    {
        return;
    }

    i32PenX = (int32_t)i16X;
    while (pcText[u16Index] != '\0')
    {
        const UiGlyph *ptGlyph;

        ptGlyph = UiFont_FindGlyph(ptFont, (uint8_t)pcText[u16Index]);
        if (ptGlyph != 0)
        {
            UiFont_DrawGlyph(i32PenX, i16BaselineY, ptFont, ptGlyph, u16Fg, u16Bg);
            i32PenX += (int32_t)ptGlyph->advance;
        }
        u16Index++;
    }
}

uint16_t UiFont_MeasureText(const UiFont *ptFont, const char *pcText)
{
    uint32_t u32Width = 0u;
    uint16_t u16Index = 0u;

    if ((ptFont == 0) || (pcText == 0))
    {
        return 0u;
    }

    while (pcText[u16Index] != '\0')
    {
        const UiGlyph *ptGlyph;

        ptGlyph = UiFont_FindGlyph(ptFont, (uint8_t)pcText[u16Index]);
        if (ptGlyph != 0)
        {
            u32Width += ptGlyph->advance;
        }
        u16Index++;
    }

    if (u32Width > 0xFFFFu)
    {
        u32Width = 0xFFFFu;
    }

    return (uint16_t)u32Width;
}
