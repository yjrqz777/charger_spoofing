/**
 * @file bsp_spi.c
 * @brief Implements GPIO-driven SPI Mode 2 transfers for the LCD.
 * @details PA5 generates SCK and PA7 generates MOSI. The SPI1 peripheral is
 *          intentionally unused to isolate peripheral configuration issues.
 */

#include "bsp_spi.h"
#include "ch32x035_rcc.h"
#include "ch32x035_gpio.h"

/**
 * @brief Initializes the LCD GPIO signals for static testing or software SPI.
 */
void BspSpiInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint32_t LcdPinMask;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

#if LCD_IO_STATIC_TEST_ENABLE
    LcdPinMask = LCD_RES_PIN | LCD_DC_PIN | LCD_CS_PIN |
                 LCD_SCK_PIN | LCD_SDA_PIN;

#if LCD_IO_STATIC_TEST_LEVEL
    GPIO_SetBits(GPIOA, LcdPinMask);
#else
    GPIO_ResetBits(GPIOA, LcdPinMask);
#endif

    GPIO_InitStructure.GPIO_Pin = LcdPinMask;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Apply the requested level again after mode configuration. */
#if LCD_IO_STATIC_TEST_LEVEL
    GPIO_SetBits(GPIOA, LcdPinMask);
#else
    GPIO_ResetBits(GPIOA, LcdPinMask);
#endif
    printf("[LCD-IO-TEST] RES/DC/CS/SCK/SDA forced %s\r\n",
           (LCD_IO_STATIC_TEST_LEVEL != 0u) ? "HIGH" : "LOW");
    printf("[LCD-IO-TEST] GPIOA CFGLR=%08lx OUTDR=%04lx lcd_bits=%04lx expected=%04lx\r\n",
           (unsigned long)GPIOA->CFGLR, (unsigned long)GPIOA->OUTDR,
           (unsigned long)(GPIOA->OUTDR & LcdPinMask),
           (LCD_IO_STATIC_TEST_LEVEL != 0u) ? (unsigned long)LcdPinMask : 0ul);
#else
    LcdPinMask = LCD_SCK_PIN | LCD_SDA_PIN;

    /* Match the proven display project: Mode 2 idles SCK high. */
    GPIO_SetBits(LCD_SCK_PORT, LCD_SCK_PIN);
    GPIO_ResetBits(LCD_SDA_PORT, LCD_SDA_PIN);

    GPIO_InitStructure.GPIO_Pin = LcdPinMask;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_SCK_PORT, &GPIO_InitStructure);

    printf("[SPI] GPIO software mode2, SCK=PA5 MOSI=PA7\r\n");
    printf("[SPI] GPIOA CFGLR=%08lx OUTDR=%04lx\r\n",
           (unsigned long)GPIOA->CFGLR, (unsigned long)GPIOA->OUTDR);
#endif
}

/**
 * @brief Sends one byte over software SPI Mode 2.
 * @param[in] u8Data Byte to send, most significant bit first.
 */
void BspSpiWriteByte(uint8_t u8Data)
{
#if LCD_IO_STATIC_TEST_ENABLE
    (void)u8Data;
#else
    uint8_t Mask;

    for (Mask = 0x80u; Mask != 0u; Mask >>= 1u)
    {
        if ((u8Data & Mask) != 0u)
        {
            GPIO_SetBits(LCD_SDA_PORT, LCD_SDA_PIN);
        }
        else
        {
            GPIO_ResetBits(LCD_SDA_PORT, LCD_SDA_PIN);
        }

        /* Mode 2: the leading falling edge clocks data into the LCD. */
        GPIO_ResetBits(LCD_SCK_PORT, LCD_SCK_PIN);
        GPIO_SetBits(LCD_SCK_PORT, LCD_SCK_PIN);
    }
#endif
}

/**
 * @brief Sends a byte buffer over software SPI.
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
 * @brief Reports the software SPI error state.
 * @retval 0 Software SPI has no peripheral timeout state.
 */
uint8_t BspSpiHasError(void)
{
    return 0u;
}
