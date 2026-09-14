/**
 * @file    bsp_lcd.c
 * @brief   LCD 显示底层驱动实现
 *******************************************************************************
 * @note    封装 ST7789V 驱动 API，简化应用层调用
 *******************************************************************************
 */

#include "bsp_lcd.h"
#include "st7789v/st7789v.h"

/**
 * @brief  初始化 LCD 显示屏
 * @note   调用 ST7789V 初始化序列，包括：
 *         - 硬件复位
 *         - 睡眠模式退出
 *         - 显示方向设置
 *         - Gamma 校正
 *         - 显示开启
 *         初始化完成后显示白色背景
 */
void BspLcdInit(void)
{
    st7789v_init();
}

/**
 * @brief  在 LCD 指定位置显示无符号整数
 * @param[in] u16X      横坐标（像素）
 * @param[in] u16Y      纵坐标（像素）
 * @param[in] u32Value  待显示的无符号整数值
 * @param[in] u8Length  显示位数
 * @note   使用白色字体、黑色背景、24 号字显示
 *         不显示前导零，不足 u8Length 位时空格填充
 * @see    LCD_ShowIntNum
 */
HAL_StatusTypeDef BspLcdShowUInt(uint16_t u16X, uint16_t u16Y, uint32_t u32Value, uint8_t u8Length)
{
    return BspLcdShowUIntColor(u16X, u16Y, u32Value, u8Length, BLACK);
}

HAL_StatusTypeDef BspLcdShowUIntColor(uint16_t u16X, uint16_t u16Y, uint32_t u32Value,
                                      uint8_t u8Length, uint16_t u16Color)
{
    return LCD_ShowIntNumDma(u16X, u16Y, u32Value, u8Length, u16Color, WHITE, 24);
}

/**
 * @brief  在 LCD 指定位置显示浮点数（保留 3 位小数）
 * @param[in] u16X      横坐标（像素）
 * @param[in] u16Y      纵坐标（像素）
 * @param[in] f32Value  待显示的浮点数值（支持负数）
 * @param[in] u8Length  总字符宽度（含小数点与符号位）
 * @param[in] u16Color  字体颜色
 * @note   使用白色背景、24 号字；u8Length 需容纳 "[-]NNN.NNN" 格式
 */
HAL_StatusTypeDef BspLcdShowFloatColor(uint16_t u16X, uint16_t u16Y, float f32Value,
                                       uint8_t u8Length, uint16_t u16Color)
{
    return LCD_ShowFloatNumDma(u16X, u16Y, f32Value, u8Length, 3u, u16Color, WHITE, 24);
}

/* ===================== 字段缓冲式刷新 ===================== */

#define BSP_LCD_FIELD_MAX (8u)

/** @brief 字段类型标识 */
typedef enum
{
    E_BSP_LCD_FIELD_UINT = 0u,   /**< 无符号整数 */
    E_BSP_LCD_FIELD_FLOAT = 1u   /**< 浮点数 */
} eBspLcdFieldTypeDef;

/** @brief 待显示字段描述 */
typedef struct tBspLcdFieldDef
{
    float f32Value;            /**< 浮点数值（E_BSP_LCD_FIELD_FLOAT 时有效） */
    uint32_t u32Value;         /**< 无符号整数值（E_BSP_LCD_FIELD_UINT 时有效） */
    uint16_t u16X;             /**< 横坐标 */
    uint16_t u16Y;             /**< 纵坐标 */
    uint16_t u16Color;         /**< 字体颜色 */
    uint8_t u8Length;          /**< 显示字符宽度 */
    uint8_t u8FieldType;       /**< 字段类型，见 eBspLcdFieldTypeDef */
} tBspLcdFieldDef;

static tBspLcdFieldDef atLcdRequestedField[BSP_LCD_FIELD_MAX];
static tBspLcdFieldDef atLcdActiveField[BSP_LCD_FIELD_MAX];
static uint8_t u8LcdRequestedFieldCount = 0u;
static uint8_t u8LcdActiveFieldCount = 0u;
static uint8_t u8LcdRefreshRequested = 0u;
static uint8_t u8LcdRefreshPending = 0u;
static uint8_t u8LcdActiveFieldIndex = 0u;
static uint8_t u8LcdRequestedStateId = 0xFFu;
static uint8_t u8LcdActiveStateId = 0xFFu;

/**
 * @brief  开启一轮字段刷新收集
 * @param[in] u8StateId  当前状态标识（仅用于检测状态切换时丢弃过期请求）
 */
void BspLcdBeginRefresh(uint8_t u8StateId)
{
    u8LcdRequestedFieldCount = 0u;
    u8LcdRequestedStateId = u8StateId;
    u8LcdRefreshRequested = 1u;
}

/**
 * @brief  向当前刷新帧追加无符号整数字段
 * @note   超过 BSP_LCD_FIELD_MAX 后的追加将被忽略
 */
void BspLcdAddUInt(uint16_t u16X, uint16_t u16Y, uint32_t u32Value,
                   uint8_t u8Length, uint16_t u16Color)
{
    if (u8LcdRequestedFieldCount < BSP_LCD_FIELD_MAX)
    {
        atLcdRequestedField[u8LcdRequestedFieldCount].u16X = u16X;
        atLcdRequestedField[u8LcdRequestedFieldCount].u16Y = u16Y;
        atLcdRequestedField[u8LcdRequestedFieldCount].u32Value = u32Value;
        atLcdRequestedField[u8LcdRequestedFieldCount].f32Value = 0.0f;
        atLcdRequestedField[u8LcdRequestedFieldCount].u16Color = u16Color;
        atLcdRequestedField[u8LcdRequestedFieldCount].u8Length = u8Length;
        atLcdRequestedField[u8LcdRequestedFieldCount].u8FieldType = E_BSP_LCD_FIELD_UINT;
        u8LcdRequestedFieldCount++;
    }
}

/**
 * @brief  向当前刷新帧追加浮点数字段（显示时保留 3 位小数）
 * @note   超过 BSP_LCD_FIELD_MAX 后的追加将被忽略
 */
void BspLcdAddFloat(uint16_t u16X, uint16_t u16Y, float f32Value,
                    uint8_t u8Length, uint16_t u16Color)
{
    if (u8LcdRequestedFieldCount < BSP_LCD_FIELD_MAX)
    {
        atLcdRequestedField[u8LcdRequestedFieldCount].u16X = u16X;
        atLcdRequestedField[u8LcdRequestedFieldCount].u16Y = u16Y;
        atLcdRequestedField[u8LcdRequestedFieldCount].f32Value = f32Value;
        atLcdRequestedField[u8LcdRequestedFieldCount].u32Value = 0u;
        atLcdRequestedField[u8LcdRequestedFieldCount].u16Color = u16Color;
        atLcdRequestedField[u8LcdRequestedFieldCount].u8Length = u8Length;
        atLcdRequestedField[u8LcdRequestedFieldCount].u8FieldType = E_BSP_LCD_FIELD_FLOAT;
        u8LcdRequestedFieldCount++;
    }
}

/**
 * @brief  字段刷新服务：按 DMA 就绪节奏逐字段输出
 * @param[in] u8StateId  当前状态标识，与发起刷新时的标识不一致时丢弃过期帧
 * @note   每次 DMA 传输完成后（返回 HAL_OK）推进到下一字段，
 *         整帧输出完毕后清除挂起标志，等待下一轮 BeginRefresh
 */
void BspLcdService(uint8_t u8StateId)
{
    uint8_t u8Index;

    if ((u8LcdRefreshRequested != 0u) && (u8LcdRequestedStateId != u8StateId))
    {
        u8LcdRefreshRequested = 0u;
    }

    if ((u8LcdRefreshPending != 0u) && (u8LcdActiveStateId != u8StateId))
    {
        u8LcdRefreshPending = 0u;
        u8LcdActiveFieldIndex = 0u;
    }

    if ((u8LcdRefreshPending == 0u) && (u8LcdRefreshRequested != 0u))
    {
        for (u8Index = 0u; u8Index < u8LcdRequestedFieldCount; u8Index++)
        {
            atLcdActiveField[u8Index] = atLcdRequestedField[u8Index];
        }
        u8LcdActiveFieldCount = u8LcdRequestedFieldCount;
        u8LcdActiveFieldIndex = 0u;
        u8LcdActiveStateId = u8LcdRequestedStateId;
        u8LcdRefreshPending = (u8LcdActiveFieldCount != 0u) ? 1u : 0u;
        u8LcdRefreshRequested = 0u;
    }

    if (u8LcdRefreshPending != 0u)
    {
        HAL_StatusTypeDef eStatus;

        if (atLcdActiveField[u8LcdActiveFieldIndex].u8FieldType == E_BSP_LCD_FIELD_FLOAT)
        {
            eStatus = BspLcdShowFloatColor(atLcdActiveField[u8LcdActiveFieldIndex].u16X,
                                           atLcdActiveField[u8LcdActiveFieldIndex].u16Y,
                                           atLcdActiveField[u8LcdActiveFieldIndex].f32Value,
                                           atLcdActiveField[u8LcdActiveFieldIndex].u8Length,
                                           atLcdActiveField[u8LcdActiveFieldIndex].u16Color);
        }
        else
        {
            eStatus = BspLcdShowUIntColor(atLcdActiveField[u8LcdActiveFieldIndex].u16X,
                                          atLcdActiveField[u8LcdActiveFieldIndex].u16Y,
                                          atLcdActiveField[u8LcdActiveFieldIndex].u32Value,
                                          atLcdActiveField[u8LcdActiveFieldIndex].u8Length,
                                          atLcdActiveField[u8LcdActiveFieldIndex].u16Color);
        }

        if (eStatus == HAL_OK)
        {
            u8LcdActiveFieldIndex++;
            if (u8LcdActiveFieldIndex >= u8LcdActiveFieldCount)
            {
                u8LcdRefreshPending = 0u;
            }
        }
    }
}
