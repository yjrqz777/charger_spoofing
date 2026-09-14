/**
 * @file bsp_spi.c
 * @brief Implements polling SPI1 transfers for the LCD.
 * @details SPI1 uses PA5 as SCK and PA7 as MOSI in one-line transmit mode.
 */

#include "bsp_spi.h"
#include "ch32x035_spi.h"
#include "ch32x035_rcc.h"
#include "ch32x035_gpio.h"

#define BSP_SPI_TIMEOUT_COUNT (0x100000u) /* Polling timeout limit. */

/** @brief SPI polling error flag. */
static uint8_t s_u8SpiError = 0u;

/**
 * @brief Initializes the LCD SPI1 interface in polling mode.
 */
void BspSpiInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));
    memset(&SPI_InitStructure, 0, sizeof(SPI_InitStructure));
    s_u8SpiError = 0u;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_AFIO |
                           RCC_APB2Periph_SPI1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = LCD_SCK_PIN | LCD_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_SCK_PORT, &GPIO_InitStructure);

    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7u;
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);
    SPI_Cmd(SPI1, ENABLE);

    printf("[SPI] SPI1 polling mode: 1-line TX mode0 /16, SCK=PA5 MOSI=PA7\r\n");
    printf("[SPI] CTLR1=%04x CTLR2=%04x STATR=%04x\r\n",
           (unsigned int)SPI1->CTLR1, (unsigned int)SPI1->CTLR2,
           (unsigned int)SPI1->STATR);
    printf("[SPI] GPIOA CFGLR=%08lx OUTDR=%04lx\r\n",
           (unsigned long)GPIOA->CFGLR, (unsigned long)GPIOA->OUTDR);
}

/**
 * @brief Sends one byte and waits until it leaves the SPI shift register.
 * @param[in] u8Data Byte to send.
 */
void BspSpiWriteByte(uint8_t u8Data)
{
    uint32_t Timeout = BSP_SPI_TIMEOUT_COUNT;

    if (s_u8SpiError != 0u)
    {
        return;
    }

    while ((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) &&
           (Timeout != 0u))
    {
        Timeout--;
    }

    if (Timeout == 0u)
    {
        s_u8SpiError = 1u;
        printf("[SPI][ERR] TXE timeout, STATR=%04x\r\n", (unsigned int)SPI1->STATR);
        return;
    }

    SPI_I2S_SendData(SPI1, u8Data);

    Timeout = BSP_SPI_TIMEOUT_COUNT;
    while ((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET) &&
           (Timeout != 0u))
    {
        Timeout--;
    }

    if (Timeout == 0u)
    {
        s_u8SpiError = 1u;
        printf("[SPI][ERR] BSY timeout, STATR=%04x\r\n", (unsigned int)SPI1->STATR);
    }
}

/**
 * @brief Sends a byte buffer using polling transfers.
 * @param[in] pu8Data Pointer to the source buffer.
 * @param[in] u16Len Number of bytes to send.
 */
void BspSpiWriteBufferBlocking(const uint8_t *pu8Data, uint16_t u16Len)
{
    uint16_t Index;

    if (pu8Data == 0)
    {
        return;
    }

    for (Index = 0u; Index < u16Len; Index++)
    {
        BspSpiWriteByte(pu8Data[Index]);
    }
}

/**
 * @brief Reports whether a polling transfer timed out.
 * @retval 1 A polling timeout occurred.
 * @retval 0 No polling timeout occurred.
 */
uint8_t BspSpiHasError(void)
{
    return s_u8SpiError;
}
