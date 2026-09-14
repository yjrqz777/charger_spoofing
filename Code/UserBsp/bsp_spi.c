/**
 * @file    bsp_spi.c
 * @brief   LCD 专用 SPI1 底层驱动实现
 *******************************************************************************
 * @note    关键设计：
 *          1) SPI1 只用 TX，配置为 1 线只发（SPI_Direction_1Line_Tx）可避免
 *             读溢出干扰；但为兼容保留全双工语义，此处使用 2 线全双工 + 只发。
 *          2) DMA 用于刷屏：一次传输完成后由 DMA1_Channel3 中断置空闲标志，
 *             应用层（bsp_lcd.c 的字段状态机）据此推进下一个字段。
 *          3) 传输前必须清 TC 标志，否则 DMA_Cmd 使能后请求无法产生。
 *******************************************************************************
 */

#include "bsp_spi.h"
#include "ch32x035_spi.h"
#include "ch32x035_dma.h"
#include "ch32x035_rcc.h"
#include "ch32x035_misc.h"
#include "ch32x035_gpio.h"

/** @brief DMA 空闲标志：1 = 可发起新传输，0 = 正在传输 */
static volatile uint8_t s_u8SpiDmaIdle = 1u;

/** @brief 单字节阻塞发送的超时计数上限（防止外设异常时死等） */
#define BSP_SPI_TIMEOUT_COUNT (0x100000u)

void BspSpiInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;
    DMA_InitTypeDef  DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));
    memset(&SPI_InitStructure,  0, sizeof(SPI_InitStructure));
    memset(&DMA_InitStructure,  0, sizeof(DMA_InitStructure));
    memset(&NVIC_InitStructure, 0, sizeof(NVIC_InitStructure));

    /* 1) 时钟：SPI1 在 APB2；DMA1 在 AHB */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    /* 2) SCK(PA4) / MOSI(PA7) 复用推挽输出
     *    注意：默认复用功能，无需 AFIO 重映射 */
    GPIO_InitStructure.GPIO_Pin   = LCD_SCK_PIN | LCD_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_SCK_PORT, &GPIO_InitStructure);

    /* 3) SPI1 主机模式配置
     *    CPOL=0 / CPHA=0 -> SPI Mode 0（SCK 空闲低、上升沿采样），ST7789V 常用模式
     *    速率：48MHz / 4 = 12MHz */
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    /* 4) 软件 NSS 置高（内部 NSS = 1，主机模式必须） */
    SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);

    /* 5) 使能 SPI1 */
    SPI_Cmd(SPI1, ENABLE);

    /* 6) DMA1 通道 3 = SPI1_TX（固定映射） */
    DMA_DeInit(DMA1_Channel3);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(SPI1->DATAR));
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)0;              /* 每次传输前重设 */
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;    /* 内存 -> 外设 */
    DMA_InitStructure.DMA_BufferSize         = 0;                        /* 每次传输前重设 */
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);

    /* 7) DMA 传输完成中断 */
    DMA_ITConfig(DMA1_Channel3, DMA_IT_TC, ENABLE);
    NVIC_InitStructure.NVIC_IRQChannel                   = DMA1_Channel3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 8) SPI 的 DMA 发送请求先关闭，由 BspSpiWriteBufferDma 按需开启 */
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, DISABLE);
}

void BspSpiWriteByte(uint8_t u8Data)
{
    uint32_t u32Timeout = BSP_SPI_TIMEOUT_COUNT;

    /* 等待发送缓冲区空 */
    while ((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) && (u32Timeout != 0u))
    {
        u32Timeout--;
    }

    SPI_I2S_SendData(SPI1, u8Data);

    /* 等待本字节真正移位完成，保证 CS 拉高前时序完整 */
    u32Timeout = BSP_SPI_TIMEOUT_COUNT;
    while ((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET) && (u32Timeout != 0u))
    {
        u32Timeout--;
    }
}

void BspSpiWriteBufferBlocking(const uint8_t *pu8Data, uint16_t u16Len)
{
    uint16_t u16Index;

    if (pu8Data == 0)
    {
        return;
    }

    for (u16Index = 0u; u16Index < u16Len; u16Index++)
    {
        BspSpiWriteByte(pu8Data[u16Index]);
    }
}

uint8_t BspSpiWriteBufferDma(const uint8_t *pu8Data, uint16_t u16Len)
{
    if ((pu8Data == 0) || (u16Len == 0u) || (s_u8SpiDmaIdle == 0u))
    {
        return 1u;
    }

    s_u8SpiDmaIdle = 0u;

    /* 关通道 -> 改参数 -> 清标志 -> 开通道 */
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_SetCurrDataCounter(DMA1_Channel3, u16Len);
    DMA1_Channel3->MADDR = (uint32_t)pu8Data;

    DMA_ClearITPendingBit(DMA1_IT_TC3);   /* 同时清 INTFR 的 GIF 与 CHINTFR 的 TCIF */

    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA1_Channel3, ENABLE);

    return 0u;
}

uint8_t BspSpiIsIdle(void)
{
    return s_u8SpiDmaIdle;
}

/**
 * @brief  DMA1 通道 3 传输完成中断（SPI1_TX）
 * @note   置空闲标志，并关闭本轮 DMA 请求；同时等待 SPI 移位完成，
 *         以便应用层可以安全地拉高 CS。
 */
void DMA1_Channel3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel3_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC3) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC3);

        DMA_Cmd(DMA1_Channel3, DISABLE);
        SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, DISABLE);

        /* 等待最后一字节移出（BSY 清零），随后应用层可拉高 CS */
        while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET)
        {
        }

        s_u8SpiDmaIdle = 1u;
    }
}
