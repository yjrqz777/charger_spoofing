/**
 * @file    bsp_lcd.c
 * @brief   LCD 显示底层驱动实现
 *******************************************************************************
 * @note    字段缓冲式刷新的实现思路：
 *          1) 应用层调用 BspLcdBeginRefresh() 开启一帧，随后用 BspLcdAddXxx()
 *             追加待显示内容，追加只写内存，不碰 SPI；
 *          2) 每个时间片调用 BspLcdService()，它只在"当前操作已完成"时推进
 *             到下一条操作，因此单次调用耗时可忽略，不会拉长 time slice；
 *          3) 浮点/整数走 DMA 异步输出；字符串与清屏为阻塞输出（耗时可控）。
 *******************************************************************************
 */

#include "bsp_lcd.h"
#include "bsp_spi.h"
#include "bsp_tick.h"
#include "st7789v/st7789v.h"

/** @brief 一帧内最多允许的待显示操作数 */
#define BSP_LCD_FIELD_MAX        (12u)

/** @brief 单条操作的最长等待时间（ms），超时则放弃本帧，防止界面卡死 */
#define BSP_LCD_FIELD_TIMEOUT_MS (500u)

/** @brief 字段操作类型 */
typedef enum
{
    E_BSP_LCD_OP_NONE = 0,   /**< 空操作（占位） */
    E_BSP_LCD_OP_FILL,       /**< 区域填充 */
    E_BSP_LCD_OP_STRING,     /**< 字符串 */
    E_BSP_LCD_OP_UINT,       /**< 无符号整数（DMA） */
    E_BSP_LCD_OP_FLOAT       /**< 浮点数（DMA） */
} eBspLcdOpTypeDef;

/** @brief 待显示操作描述 */
typedef struct tBspLcdOpDef
{
    uint16_t        u16X;        /**< 横坐标 */
    uint16_t        u16Y;        /**< 纵坐标 */
    uint16_t        u16Fc;       /**< 前景色 */
    uint16_t        u16Bc;       /**< 背景色 */
    uint8_t         u8Type;      /**< 操作类型，见 eBspLcdOpTypeDef */
    uint8_t         u8Length;    /**< 数字位宽 */
    uint8_t         u8Decimals;  /**< 小数位数 */
    uint8_t         u8SizeY;     /**< 字号 */
    uint8_t         u8TextLen;   /**< 字符串有效长度（不含结尾 0） */
    uint32_t        u32Value;    /**< 无符号整数值 */
    float           f32Value;    /**< 浮点数值 */
    char            acText[18];  /**< 字符串缓冲 */
} tBspLcdOpDef;

/* ---- 请求帧与正在输出的活动帧 ---- */
static tBspLcdOpDef s_atRequestedOp[BSP_LCD_FIELD_MAX];
static tBspLcdOpDef s_atActiveOp[BSP_LCD_FIELD_MAX];

static uint8_t  s_u8RequestedCount  = 0u;      /**< 请求帧中的操作数 */
static uint8_t  s_u8ActiveCount     = 0u;      /**< 活动帧中的操作数 */
static uint8_t  s_u8ActiveIndex     = 0u;      /**< 活动帧当前操作下标 */
static uint8_t  s_u8RefreshRequested = 0u;     /**< 有新的请求帧待接管 */
static uint8_t  s_u8RefreshInFlight  = 0u;     /**< 正在输出活动帧 */
static uint8_t  s_u8RequestedStateId = 0xFFu;  /**< 请求帧对应的系统状态 */
static uint8_t  s_u8ActiveStateId    = 0xFFu;  /**< 活动帧对应的系统状态 */
static uint32_t s_u32OpStartMs       = 0u;     /**< 当前操作开始时刻（用于超时保护） */

/* ========================================================================== *
 *  内部函数
 * ========================================================================== */

/**
 * @brief  阻塞等待当前 LCD DMA 传输结束
 * @param[in] u32TimeoutMs  超时时间（ms）
 * @retval 0  已空闲
 * @retval 1  超时
 */
static uint8_t BspLcdWaitDmaIdle(uint32_t u32TimeoutMs)
{
    uint32_t u32Start = BspTickGetMs();

    while (LCD_IsDmaBusy() != 0u)
    {
        if ((BspTickGetMs() - u32Start) > u32TimeoutMs)
        {
            return 1u;
        }
    }

    return 0u;
}

/**
 * @brief  输出一条操作到 LCD
 * @param[in] ptOp  操作描述
 * @retval 0  已发起输出（可能需要等待 DMA 完成）
 * @retval 1  失败
 */
static uint8_t BspLcdOutputOp(const tBspLcdOpDef *ptOp)
{
    switch (ptOp->u8Type)
    {
        case E_BSP_LCD_OP_FILL:
            LCD_Fill(0u, 0u, LCD_W, LCD_H, ptOp->u16Fc);
            return 0u;

        case E_BSP_LCD_OP_STRING:
            LCD_ShowString(ptOp->u16X, ptOp->u16Y, (const uint8_t *)ptOp->acText,
                           ptOp->u16Fc, ptOp->u16Bc, ptOp->u8SizeY, 0u);
            return 0u;

        case E_BSP_LCD_OP_UINT:
            if (LCD_ShowIntNumDma(ptOp->u16X, ptOp->u16Y, ptOp->u32Value,
                                  ptOp->u8Length, ptOp->u16Fc, ptOp->u16Bc,
                                  ptOp->u8SizeY) != HAL_OK)
            {
                return 1u;
            }
            return 0u;

        case E_BSP_LCD_OP_FLOAT:
            if (LCD_ShowFloatNumDma(ptOp->u16X, ptOp->u16Y, ptOp->f32Value,
                                    ptOp->u8Length, ptOp->u8Decimals,
                                    ptOp->u16Fc, ptOp->u16Bc,
                                    ptOp->u8SizeY) != HAL_OK)
            {
                return 1u;
            }
            return 0u;

        default:
            return 0u;
    }
}

/**
 * @brief  判断一条操作是否已经输出完成
 * @param[in] ptOp  操作描述
 * @retval 1  完成
 * @retval 0  仍在进行
 * @note   填充与字符串为阻塞输出，返回时即已完成，恒为"完成"；
 *         整数/浮点为 DMA 异步输出，需等待 LCD 的 DMA 忙标志清零。
 *         （DMA 忙标志由 bsp_spi.c 的 DMA1_Channel3 中断维护，
 *           并在 st7789v.c 的 LCD_IsDmaBusy() 中叠加字段级状态。）
 */
static uint8_t BspLcdOpFinished(const tBspLcdOpDef *ptOp)
{
    (void)ptOp;

    return (LCD_IsDmaBusy() == 0u) ? 1u : 0u;
}

/**
 * @brief  追加一条操作到请求帧
 * @return 指向新操作的空槽；队列满时返回 0
 */
static tBspLcdOpDef *BspLcdAllocOp(void)
{
    tBspLcdOpDef *ptOp;

    if (s_u8RequestedCount >= BSP_LCD_FIELD_MAX)
    {
        return 0;
    }

    ptOp = &s_atRequestedOp[s_u8RequestedCount];
    s_u8RequestedCount++;

    memset(ptOp, 0, sizeof(tBspLcdOpDef));

    return ptOp;
}

/* ========================================================================== *
 *  对外接口
 * ========================================================================== */

void BspLcdInit(void)
{
    uint8_t u8Index;

    for (u8Index = 0u; u8Index < BSP_LCD_FIELD_MAX; u8Index++)
    {
        memset(&s_atRequestedOp[u8Index], 0, sizeof(tBspLcdOpDef));
        memset(&s_atActiveOp[u8Index],    0, sizeof(tBspLcdOpDef));
    }

    s_u8RequestedCount   = 0u;
    s_u8ActiveCount      = 0u;
    s_u8ActiveIndex      = 0u;
    s_u8RefreshRequested = 0u;
    s_u8RefreshInFlight  = 0u;
    s_u8RequestedStateId = 0xFFu;
    s_u8ActiveStateId    = 0xFFu;

    /* ST7789V 初始化（内部含复位时序与整屏填充） */
    st7789v_init();
}

void BspLcdClearScreen(uint16_t u16Color)
{
    LCD_Fill(0u, 0u, LCD_W, LCD_H, u16Color);
}

void BspLcdShowString(uint16_t u16X, uint16_t u16Y, const char *pcText,
                      uint16_t u16Fc, uint16_t u16Bc, uint8_t u8SizeY, uint8_t u8Mode)
{
    if (pcText == 0)
    {
        return;
    }

    LCD_ShowString(u16X, u16Y, (const uint8_t *)pcText, u16Fc, u16Bc, u8SizeY, u8Mode);
}

void BspLcdShowUInt(uint16_t u16X, uint16_t u16Y, uint32_t u32Value,
                    uint8_t u8Length, uint16_t u16Fc, uint16_t u16Bc)
{
    if (BspLcdWaitDmaIdle(BSP_LCD_FIELD_TIMEOUT_MS) != 0u)
    {
        return;
    }

    (void)LCD_ShowIntNumDma(u16X, u16Y, u32Value, u8Length, u16Fc, u16Bc, 24u);
}

void BspLcdShowFloat(uint16_t u16X, uint16_t u16Y, float f32Value, uint8_t u8Length,
                     uint8_t u8Decimals, uint16_t u16Fc, uint16_t u16Bc)
{
    if (BspLcdWaitDmaIdle(BSP_LCD_FIELD_TIMEOUT_MS) != 0u)
    {
        return;
    }

    (void)LCD_ShowFloatNumDma(u16X, u16Y, f32Value, u8Length, u8Decimals, u16Fc, u16Bc, 24u);
}

void BspLcdBeginRefresh(uint8_t u8StateId)
{
    s_u8RequestedCount   = 0u;
    s_u8RequestedStateId = u8StateId;
    s_u8RefreshRequested = 1u;
}

void BspLcdAddFill(uint16_t u16Color)
{
    tBspLcdOpDef *ptOp = BspLcdAllocOp();

    if (ptOp == 0)
    {
        return;
    }

    ptOp->u8Type = (uint8_t)E_BSP_LCD_OP_FILL;
    ptOp->u16Fc  = u16Color;
}

void BspLcdAddString(uint16_t u16X, uint16_t u16Y, const char *pcText,
                     uint16_t u16Fc, uint16_t u16Bc, uint8_t u8SizeY)
{
    tBspLcdOpDef *ptOp;
    uint8_t       u8Index = 0u;

    if (pcText == 0)
    {
        return;
    }

    ptOp = BspLcdAllocOp();
    if (ptOp == 0)
    {
        return;
    }

    ptOp->u8Type  = (uint8_t)E_BSP_LCD_OP_STRING;
    ptOp->u16X    = u16X;
    ptOp->u16Y    = u16Y;
    ptOp->u16Fc   = u16Fc;
    ptOp->u16Bc   = u16Bc;
    ptOp->u8SizeY = u8SizeY;

    /* 拷贝字符串（含长度截断保护），不保存调用者的指针，避免悬空 */
    while ((pcText[u8Index] != '\0') && (u8Index < (uint8_t)(sizeof(ptOp->acText) - 1u)))
    {
        ptOp->acText[u8Index] = pcText[u8Index];
        u8Index++;
    }
    ptOp->acText[u8Index] = '\0';
    ptOp->u8TextLen = u8Index;
}

void BspLcdAddUInt(uint16_t u16X, uint16_t u16Y, uint32_t u32Value,
                   uint8_t u8Length, uint16_t u16Color)
{
    tBspLcdOpDef *ptOp = BspLcdAllocOp();

    if (ptOp == 0)
    {
        return;
    }

    ptOp->u8Type    = (uint8_t)E_BSP_LCD_OP_UINT;
    ptOp->u16X      = u16X;
    ptOp->u16Y      = u16Y;
    ptOp->u32Value  = u32Value;
    ptOp->u16Fc     = u16Color;
    ptOp->u16Bc     = WHITE;
    ptOp->u8Length  = u8Length;
    ptOp->u8SizeY   = 24u;
}

void BspLcdAddFloat(uint16_t u16X, uint16_t u16Y, float f32Value,
                    uint8_t u8Length, uint8_t u8Decimals, uint16_t u16Color)
{
    tBspLcdOpDef *ptOp = BspLcdAllocOp();

    if (ptOp == 0)
    {
        return;
    }

    ptOp->u8Type     = (uint8_t)E_BSP_LCD_OP_FLOAT;
    ptOp->u16X       = u16X;
    ptOp->u16Y       = u16Y;
    ptOp->f32Value   = f32Value;
    ptOp->u16Fc      = u16Color;
    ptOp->u16Bc      = WHITE;
    ptOp->u8Length   = u8Length;
    ptOp->u8Decimals = u8Decimals;
    ptOp->u8SizeY    = 24u;
}

void BspLcdService(uint8_t u8StateId)
{
    /* 1) 状态已切换：丢弃过期的请求帧与进行中的活动帧 */
    if ((s_u8RefreshRequested != 0u) && (s_u8RequestedStateId != u8StateId))
    {
        s_u8RefreshRequested = 0u;
        s_u8RequestedCount   = 0u;
    }

    if ((s_u8RefreshInFlight != 0u) && (s_u8ActiveStateId != u8StateId))
    {
        s_u8RefreshInFlight = 0u;
        s_u8ActiveIndex     = 0u;
        s_u8ActiveCount     = 0u;
    }

    /* 2) 空闲且有新请求：接管请求帧 */
    if ((s_u8RefreshInFlight == 0u) && (s_u8RefreshRequested != 0u))
    {
        uint8_t u8Index;

        for (u8Index = 0u; u8Index < s_u8RequestedCount; u8Index++)
        {
            s_atActiveOp[u8Index] = s_atRequestedOp[u8Index];
        }

        s_u8ActiveCount      = s_u8RequestedCount;
        s_u8ActiveIndex      = 0u;
        s_u8ActiveStateId    = s_u8RequestedStateId;
        s_u8RefreshInFlight  = (s_u8ActiveCount != 0u) ? 1u : 0u;
        s_u8RefreshRequested = 0u;
        s_u32OpStartMs       = BspTickGetMs();
    }

    /* 3) 推进当前活动帧 */
    if (s_u8RefreshInFlight != 0u)
    {
        /* 3.1 若上一条已输出完毕，则推进下标 */
        if (BspLcdOpFinished(&s_atActiveOp[s_u8ActiveIndex]) != 0u)
        {
            s_u8ActiveIndex++;
            s_u32OpStartMs = BspTickGetMs();

            if (s_u8ActiveIndex >= s_u8ActiveCount)
            {
                /* 整帧输出完成 */
                s_u8RefreshInFlight = 0u;
                s_u8ActiveIndex     = 0u;
                s_u8ActiveCount     = 0u;
                return;
            }

            /* 3.2 输出新的当前操作 */
            if (BspLcdOutputOp(&s_atActiveOp[s_u8ActiveIndex]) != 0u)
            {
                /* 输出失败（多为上一笔 DMA 未空），下个时间片重试 */
                s_u8ActiveIndex--;
            }
            return;
        }

        /* 3.3 当前操作长时间未完成：放弃本帧，防止界面永久卡住 */
        if ((BspTickGetMs() - s_u32OpStartMs) > BSP_LCD_FIELD_TIMEOUT_MS)
        {
            s_u8RefreshInFlight = 0u;
            s_u8ActiveIndex     = 0u;
            s_u8ActiveCount     = 0u;
        }
    }
}
