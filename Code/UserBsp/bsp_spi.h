/**
 * @file    bsp_spi.h
 * @brief   LCD 专用 SPI1 底层驱动头文件
 *******************************************************************************
 * @note    硬件依据：
 *            - SPI1 挂 APB2，默认复用 SCK=PA4、MOSI=PA7（数据手册 2.3 节复用表）
 *            - SPI1_TX 的 DMA 请求固定映射到 DMA1 通道 3
 *              （参考手册 §9.2.3 图9-1 DMA 请求映像）
 *            - NSS 使用软件管理，片选由 LCD_CS 引脚手动控制
 *
 *          速率：SPI1 时钟 = PCLK2 = 48MHz；预分频 4 -> 12MHz（< ST7789V 上限）
 *******************************************************************************
 */

#ifndef __BSP_SPI_H__
#define __BSP_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief  初始化 SPI1（主机、8bit、Mode0、软件 NSS）与 TX DMA（DMA1_Channel3）
 * @note   调用前需保证 GPIO 时钟已使能（由 BspBoardInit 完成）。
 */
void BspSpiInit(void);

/**
 * @brief  阻塞发送单字节
 * @param[in] u8Data  待发送字节
 * @note   内部等待 TXE 与 BSY 标志，适合命令/参数等零散字节。
 */
void BspSpiWriteByte(uint8_t u8Data);

/**
 * @brief  阻塞发送一段数据（轮询，无 DMA）
 * @param[in] pu8Data  数据首地址
 * @param[in] u16Len   字节数
 */
void BspSpiWriteBufferBlocking(const uint8_t *pu8Data, uint16_t u16Len);

/**
 * @brief  启动 DMA 异步发送一段数据
 * @param[in] pu8Data  数据首地址（必须保持有效直到传输完成）
 * @param[in] u16Len   字节数
 * @retval 0  启动成功
 * @retval 1  上一笔传输尚未完成（忙）
 * @note   本函数立即返回；传输结束后 DMA1_Channel3_IRQHandler 会置空闲标志。
 */
uint8_t BspSpiWriteBufferDma(const uint8_t *pu8Data, uint16_t u16Len);

/**
 * @brief  查询 DMA 传输是否空闲
 * @retval 1  空闲（可发起下一笔）
 * @retval 0  正在传输
 */
uint8_t BspSpiIsIdle(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SPI_H__ */
