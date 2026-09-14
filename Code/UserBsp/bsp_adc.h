/**
 * @file    bsp_adc.h
 * @brief   ADC 采样底层驱动头文件（母线电压 / 输出电压 / 输出电流）
 *******************************************************************************
 * @note    硬件依据（原理图 doc/schematic/NETLIST.md）：
 *            - IBUS-ADC    ：PA4 -> ADC1_IN4   （经 R16 100R + C11 10nF）
 *            - USB-VBUS-ADC：PC0 -> ADC1_IN10  （经 R12 100R + C10 100nF）
 *            - VOUT-ADC    ：PA0 -> ADC1_IN0   （经 R11 100R + C9 100nF）
 *          通道号依据数据手册 2.3 节复用表：引脚名后缀 Axx 即 ADC_INxx。
 *
 *          换算系数集中在 Code/main.h，依据原理图分压/增益参数：
 *            - 电压：470k / 68k 分压（≈1:7.9118）
 *            - 电流：R7 = 10mΩ + INA180A2（50 V/V），满量程 ≈ 6.6A
 *******************************************************************************
 */

#ifndef __BSP_ADC_H__
#define __BSP_ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** @brief 采样通道枚举 */
typedef enum
{
    E_BSP_ADC_IBUS = 0,   /**< 输出电流（PA3 / IN3）      */
    E_BSP_ADC_VBUS,       /**< 输入母线电压（PC0 / IN10） */
    E_BSP_ADC_VOUT,       /**< 输出电压（PC3 / IN13）     */
    E_BSP_ADC_CH_MAX      /**< 通道总数（边界标记）       */
} eBspAdcChannelDef;

/** @brief 一帧采样结果 */
typedef struct tBspAdcDataDef
{
    uint16_t u16Raw[E_BSP_ADC_CH_MAX];   /**< 原始 ADC 码值（0~4095） */
    float    f32Voltage;                 /**< 输入母线电压 USB-VBUS (V) */
    float    f32Vout;                    /**< 输出电压 VOUT (V) */
    float    f32Current;                 /**< 输出电流 IBUS (A) */
    float    f32Power;                   /**< 输出功率 VOUT x IBUS (W) */
} tBspAdcDataDef;

/** @brief 全局采样数据（由显示任务读取，由采样周期更新） */
extern tBspAdcDataDef tBspAdcData;

/**
 * @brief  初始化 ADC1（单次转换、软件触发、右对齐、12bit）
 * @note   需在 BspBoardInit() 之后调用（GPIO 已配置为模拟输入）。
 */
void BspAdcInit(void);

/**
 * @brief  读取指定通道的原始码值（阻塞轮询，单次转换）
 * @param[in]  eChannel  通道
 * @param[out] pu16Raw   结果输出
 * @retval 0  成功
 * @retval 1  超时或参数非法
 */
uint8_t BspAdcReadRaw(eBspAdcChannelDef eChannel, uint16_t *pu16Raw);

/**
 * @brief  采样全部三路并换算为物理量（含一阶 IIR 滤波）
 * @note   阻塞时间约几十微秒（3 次单通道转换），
 *         在显示任务中以 100ms 周期调用即可，不构成时间片负担。
 */
void BspAdcUpdateAll(void);

/**
 * @brief  获取最近一次采样数据
 * @return 采样数据结构体指针（只读）
 */
const tBspAdcDataDef *BspAdcGetData(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_ADC_H__ */
