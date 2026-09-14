/**
 * @file    bsp_adc.c
 * @brief   ADC 采样底层驱动实现
 *******************************************************************************
 * @note    配置要点：
 *          - ADC1 单次转换 + 软件触发 + 右对齐 + 12bit；
 *          - ADC 时钟 = PCLK2 / 6 = 48MHz / 6 = 8MHz（满足 ADC 时钟上限）；
 *          - 采样时间取 11 周期（分压电路源阻抗较高，取较长采样时间）；
 *          - 三路分时轮询：每次 BspAdcUpdateAll() 依次采 IBUS/VBUS/VOUT。
 *******************************************************************************
 */

#include "bsp_adc.h"
#include "ch32x035_adc.h"
#include "ch32x035_rcc.h"

/** @brief 全局采样数据 */
tBspAdcDataDef tBspAdcData;

/** @brief 单次转换轮询超时上限 */
#define BSP_ADC_TIMEOUT_COUNT (0x00080000u)

/**
 * @brief 一阶 IIR 低通滤波
 * @param[in] f32Old  上次输出
 * @param[in] f32New  本次采样
 * @return 滤波结果
 * @note   系数 1/8：对 100ms 采样周期而言足以压掉抖动，同时响应不迟钝。
 */
static float BspAdcFilter(float f32Old, float f32New)
{
    return (f32Old * 7.0f + f32New) / 8.0f;
}

/**
 * @brief  把通道枚举映射到 ADC 通道号
 */
static uint8_t BspAdcGetChannelNumber(eBspAdcChannelDef eChannel)
{
    uint8_t u8Channel;

    switch (eChannel)
    {
        case E_BSP_ADC_IBUS:
            u8Channel = ADC_IBUS_CHANNEL;
            break;
        case E_BSP_ADC_VBUS:
            u8Channel = ADC_VBUS_CHANNEL;
            break;
        case E_BSP_ADC_VOUT:
            u8Channel = ADC_VOUT_CHANNEL;
            break;
        default:
            u8Channel = ADC_IBUS_CHANNEL;
            break;
    }

    return u8Channel;
}

/** @brief 各通道静态标定系数（由枚举顺序索引） */
static const float s_af32CodeToUnit[E_BSP_ADC_CH_MAX] =
{
    BOARD_IBUS_CODE_TO_A,   /* E_BSP_ADC_IBUS : 码值 -> A   */
    BOARD_VBUS_CODE_TO_V,   /* E_BSP_ADC_VBUS : 码值 -> V   */
    BOARD_VOUT_CODE_TO_V    /* E_BSP_ADC_VOUT : 码值 -> V   */
};

void BspAdcInit(void)
{
    ADC_InitTypeDef ADC_InitStructure;
    uint8_t u8Index;

    memset(&ADC_InitStructure, 0, sizeof(ADC_InitStructure));
    memset(&tBspAdcData, 0, sizeof(tBspAdcData));

    /* 1) 时钟：使能 ADC1；ADC 时钟 = SYSCLK/6，48MHz 下为 8MHz */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    ADC_CLKConfig(ADC1, ADC_CLK_Div6);

    /* 2) ADC1 基础配置：独立模式、单次转换、软件触发、右对齐、1 个规则通道 */
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;   /* 单通道，不需要扫描 */
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;   /* 单次转换，软件触发 */
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 3) 使能 ADC1 并等待其稳定 */
    ADC_Cmd(ADC1, ENABLE);
    Delay_Ms(1);

    /* 4) 各通道初始化值清零 */
    for (u8Index = 0u; u8Index < (uint8_t)E_BSP_ADC_CH_MAX; u8Index++)
    {
        tBspAdcData.u16Raw[u8Index] = 0u;
    }
}

uint8_t BspAdcReadRaw(eBspAdcChannelDef eChannel, uint16_t *pu16Raw)
{
    uint32_t u32Timeout = BSP_ADC_TIMEOUT_COUNT;

    if ((pu16Raw == 0) || (eChannel >= E_BSP_ADC_CH_MAX))
    {
        return 1u;
    }

    /* 1) 配置规则通道（Rank1，采样时间 11 周期） */
    ADC_RegularChannelConfig(ADC1, BspAdcGetChannelNumber(eChannel), 1u, ADC_SampleTime_11Cycles);

    /* 2) 清 EOC 标志，避免读到上一次的残留状态 */
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);

    /* 3) 软件触发启动转换 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* 4) 轮询等待转换结束（EOC 置位） */
    while ((ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) && (u32Timeout != 0u))
    {
        u32Timeout--;
    }

    if (u32Timeout == 0u)
    {
        return 1u;
    }

    /* 5) 读结果（读 RDATAR 会清 EOC） */
    *pu16Raw = (uint16_t)(ADC_GetConversionValue(ADC1) & 0x0FFFu);

    return 0u;
}

void BspAdcUpdateAll(void)
{
    uint8_t  u8Index;
    uint16_t u16Raw;
    float    af32Value[E_BSP_ADC_CH_MAX];

    /* 1) 依次采样三路并换算为物理量 */
    for (u8Index = 0u; u8Index < (uint8_t)E_BSP_ADC_CH_MAX; u8Index++)
    {
        if (BspAdcReadRaw((eBspAdcChannelDef)u8Index, &u16Raw) == 0u)
        {
            tBspAdcData.u16Raw[u8Index] = u16Raw;
        }

        af32Value[u8Index] = (float)tBspAdcData.u16Raw[u8Index] * s_af32CodeToUnit[u8Index];
    }

    /* 2) 一阶 IIR 滤波后落盘 */
    tBspAdcData.f32Current = BspAdcFilter(tBspAdcData.f32Current, af32Value[E_BSP_ADC_IBUS]);
    tBspAdcData.f32Voltage = BspAdcFilter(tBspAdcData.f32Voltage, af32Value[E_BSP_ADC_VBUS]);
    tBspAdcData.f32Vout    = BspAdcFilter(tBspAdcData.f32Vout,    af32Value[E_BSP_ADC_VOUT]);

    /* 3) 功率 = 输出电压 x 输出电流 */
    tBspAdcData.f32Power = tBspAdcData.f32Vout * tBspAdcData.f32Current;
}

const tBspAdcDataDef *BspAdcGetData(void)
{
    return &tBspAdcData;
}
