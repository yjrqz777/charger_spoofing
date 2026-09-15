/**
 * @file    ui_font.h
 * @brief   轻量 2bpp 点阵文字渲染器头文件
 *******************************************************************************
 * @note    设计目标（见 doc/LCD_DISPLAY_IMPLEMENTATION_PLAN.md §6/§7）：
 *            - 字模在 PC 端用 tools/font_converter/generate_fonts.py 预先栅格化成
 *              2bpp 位图，固件端不解析 TTF、不做字模缩放；
 *            - 渲染时按"单个字形矩形"设置 LCD 窗口，逐字形 DMA 输出，
 *              不需要整屏 RGB565 帧缓冲；
 *            - 2bpp 覆盖度做线性混合，得到抗锯齿边缘：
 *                alpha=0 -> 背景色，alpha=3 -> 前景色，
 *                alpha=1/2 -> 分别按 1/3、2/3 混合前景与背景。
 *
 *          坐标约定：
 *            - DrawText() 的 y 参数是**基线**坐标，不是顶边；
 *            - 字形的实际位置 = (x + x_offset, baseline_y + y_offset)，
 *              其中 y_offset 为负表示墨迹位于基线上方；
 *            - 只有整数运算，不依赖浮点 printf/snprintf。
 *******************************************************************************
 */

#ifndef __UI_FONT_H__
#define __UI_FONT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** @brief 单个字形的描述（由字库生成脚本填充） */
typedef struct
{
    uint32_t bitmap_offset;   /**< 在字库位图数组中的字节偏移 */
    uint8_t  width;           /**< 字形位图宽（像素），0 表示无墨迹（如空格） */
    uint8_t  height;          /**< 字形位图高（像素），0 表示无墨迹 */
    int8_t   x_offset;        /**< 相对笔位置的横向偏移（像素） */
    int8_t   y_offset;        /**< 相对基线的纵向偏移（像素），负值表示在上方 */
    uint8_t  advance;         /**< 前进宽度（像素） */
    uint32_t codepoint;       /**< 字符码点（ASCII） */
} UiGlyph;

/** @brief 一套字库（位图 + 字形表 + 行高信息） */
typedef struct
{
    const uint8_t *bitmap;      /**< 位图数据（2bpp，逐行按字节对齐） */
    const UiGlyph *glyphs;      /**< 字形描述表 */
    uint16_t       glyph_count; /**< 字形数量 */
    uint8_t        line_height; /**< 行高（ascent + descent） */
    uint8_t        baseline;    /**< 顶边到基线的距离（ascent） */
} UiFont;

/**
 * @brief  以基线为基准绘制一段文本
 * @param[in] i16X         笔位置横坐标
 * @param[in] i16BaselineY 基线纵坐标
 * @param[in] ptFont       字库
 * @param[in] u16Fg        前景色（RGB565）
 * @param[in] u16Bg        背景色（RGB565），不透明绘制
 * @param[in] pcText       以 '\0' 结尾的字符串；不在字库中的字符会被跳过
 * @note   字形按需查找（线性扫描，字符数很少）；绘制是阻塞的，
 *         每个字形一次 SPI DMA，必须在本任务上下文调用，不能在中断里调用。
 */
void UiFont_DrawText(int16_t i16X, int16_t i16BaselineY, const UiFont *ptFont,
                     uint16_t u16Fg, uint16_t u16Bg, const char *pcText);

/**
 * @brief  测量一段文本的前进宽度
 * @param[in] ptFont  字库
 * @param[in] pcText  以 '\0' 结尾的字符串
 * @return 文本宽度（像素）；未知字符按 0 计算
 */
uint16_t UiFont_MeasureText(const UiFont *ptFont, const char *pcText);

/**
 * @brief  查询一个字形的描述
 * @param[in] ptFont     字库
 * @param[in] u8Char     字符
 * @return 字形描述指针；字符不在字库中时返回 0
 */
const UiGlyph *UiFont_FindGlyph(const UiFont *ptFont, uint8_t u8Char);

/**
 * @brief  RGB565 前景/背景按 2bpp 覆盖度混合
 * @param[in] u16Fg    前景色
 * @param[in] u16Bg    背景色
 * @param[in] u8Alpha  覆盖度 0..3
 * @return 混合后的 RGB565 颜色
 */
uint16_t UiFont_Blend565(uint16_t u16Fg, uint16_t u16Bg, uint8_t u8Alpha);

#ifdef __cplusplus
}
#endif

#endif /* __UI_FONT_H__ */
