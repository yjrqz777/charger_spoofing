/**
 * @file    bsp_board.h
 * @brief   板级初始化底层驱动头文件
 *******************************************************************************
 * @note    负责把 main.h 中的引脚定义落地为实际的 GPIO 配置：
 *          - 使能各端口时钟
 *          - LCD 控制线（RES/DC/CS）推挽输出
 *          - 按键输入（上拉输入，低有效）
 *          - VOUT-EN / LED_RUN 推挽输出并置为安全默认值
 *          - ADC 采样脚配置为模拟输入
 *******************************************************************************
 */

#ifndef __BSP_BOARD_H__
#define __BSP_BOARD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief  上电板级 GPIO 初始化
 * @note   必须在任何外设驱动初始化之前调用。
 *         初始化后系统处于安全状态：VOUT-EN 输出低（输出关断）、LED 熄灭。
 */
void BspBoardInit(void);

/**
 * @brief  设置输出使能 VOUT-EN
 * @param[in] u8Enable  0=关断输出, 非0=导通输出
 */
void BspBoardSetVoutEnable(uint8_t u8Enable);

/**
 * @brief  读取输出使能当前状态
 * @return 0=关断, 1=导通
 */
uint8_t BspBoardGetVoutEnable(void);

/**
 * @brief  设置运行指示灯
 * @param[in] u8On  0=熄灭, 非0=点亮
 */
void BspBoardSetLed(uint8_t u8On);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_BOARD_H__ */
