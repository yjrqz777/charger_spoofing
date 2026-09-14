/**
 * @file bsp_ws2812.c
 * @brief Implements WS2812 output with TIM1 channel 1 PWM and DMA1 channel 5.
 * @details PB9 carries an 800 kHz waveform. Each DMA half-word updates the
 *          TIM1_CH1 compare value for one WS2812 data bit.
 */

#include "bsp_ws2812.h"
#include "ch32x035_dma.h"
#include "ch32x035_gpio.h"
#include "ch32x035_misc.h"
#include "ch32x035_rcc.h"
#include "ch32x035_tim.h"

static uint16_t au16Ws2812PwmBuffer[BSP_WS2812_BUFFER_LENGTH] = {0u};
static volatile uint8_t u8Ws2812DmaIdle = 1u;

/**
 * @brief Encodes one color component into eight PWM compare values.
 * @param[out] pu16Destination Pointer to eight destination half-words.
 * @param[in] u8Value Color component value.
 */
static void BspWs2812EncodeByte(uint16_t *pu16Destination, uint8_t u8Value)
{
    uint8_t BitIndex;

    for (BitIndex = 0u; BitIndex < 8u; BitIndex++)
    {
        pu16Destination[BitIndex] = ((u8Value & (uint8_t)(0x80u >> BitIndex)) != 0u) ?
                                    BSP_WS2812_PWM_ONE_COMPARE :
                                    BSP_WS2812_PWM_ZERO_COMPARE;
    }
}

/**
 * @brief Initializes PB9, TIM1_CH1, and DMA1 channel 5 for WS2812 output.
 */
void BspWs2812Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));
    memset(&TIM_OCInitStructure, 0, sizeof(TIM_OCInitStructure));
    memset(&TIM_TimeBaseInitStructure, 0, sizeof(TIM_TimeBaseInitStructure));
    memset(&DMA_InitStructure, 0, sizeof(DMA_InitStructure));
    memset(&NVIC_InitStructure, 0, sizeof(NVIC_InitStructure));
    memset(au16Ws2812PwmBuffer, 0, sizeof(au16Ws2812PwmBuffer));
    u8Ws2812DmaIdle = 1u;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_TIM1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    GPIO_ResetBits(WS2812_PORT, WS2812_PIN);
    GPIO_InitStructure.GPIO_Pin = WS2812_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(WS2812_PORT, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period = BSP_WS2812_PWM_PERIOD - 1u;
    TIM_TimeBaseInitStructure.TIM_Prescaler = (uint16_t)(SystemCoreClock / 8000000u) - 1u;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0u;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_Pulse = 0u;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_DMACmd(TIM1, TIM_DMA_Update, ENABLE);
    TIM_Cmd(TIM1, DISABLE);

    DMA_DeInit(DMA1_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&TIM1->CH1CVR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)au16Ws2812PwmBuffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = BSP_WS2812_BUFFER_LENGTH;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel5, DISABLE);
    DMA_ClearITPendingBit(DMA1_IT_GL5);
    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC | DMA_IT_TE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1u;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1u;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    (void)BspWs2812Show();
    printf("[WS2812] TIM1_CH1 PB9 PWM+DMA1_CH5, pixels=%u\r\n",
           (unsigned int)BSP_WS2812_LED_COUNT);
}

/**
 * @brief Sets one pixel in the pending GRB frame.
 * @param[in] u16Index Pixel index from zero through three.
 * @param[in] u8Red Red component.
 * @param[in] u8Green Green component.
 * @param[in] u8Blue Blue component.
 * @retval E_OK The pixel was updated.
 * @retval E_BUSY The previous frame is still being transmitted.
 * @retval E_ERROR The pixel index is invalid.
 */
eStatusDef BspWs2812SetPixel(uint16_t u16Index, uint8_t u8Red,
                             uint8_t u8Green, uint8_t u8Blue)
{
    uint16_t *pu16Pixel;

    if (u16Index >= BSP_WS2812_LED_COUNT)
    {
        return E_ERROR;
    }

    if (u8Ws2812DmaIdle == 0u)
    {
        return E_BUSY;
    }

    pu16Pixel = &au16Ws2812PwmBuffer[u16Index * BSP_WS2812_BITS_PER_PIXEL];
    BspWs2812EncodeByte(pu16Pixel, u8Green);
    BspWs2812EncodeByte(&pu16Pixel[8], u8Red);
    BspWs2812EncodeByte(&pu16Pixel[16], u8Blue);

    return E_OK;
}

/**
 * @brief Sets all four pixels to the same pending color.
 * @param[in] u8Red Red component.
 * @param[in] u8Green Green component.
 * @param[in] u8Blue Blue component.
 * @retval E_OK All pixels were updated.
 * @retval E_BUSY The previous frame is still being transmitted.
 */
eStatusDef BspWs2812Fill(uint8_t u8Red, uint8_t u8Green, uint8_t u8Blue)
{
    uint16_t PixelIndex;

    if (u8Ws2812DmaIdle == 0u)
    {
        return E_BUSY;
    }

    for (PixelIndex = 0u; PixelIndex < BSP_WS2812_LED_COUNT; PixelIndex++)
    {
        (void)BspWs2812SetPixel(PixelIndex, u8Red, u8Green, u8Blue);
    }

    return E_OK;
}

/**
 * @brief Starts asynchronous transmission of the pending pixel frame.
 * @retval E_OK The PWM DMA transfer was started.
 * @retval E_BUSY A previous frame is still being transmitted.
 */
eStatusDef BspWs2812Show(void)
{
    if (u8Ws2812DmaIdle == 0u)
    {
        return E_BUSY;
    }

    u8Ws2812DmaIdle = 0u;
    TIM_Cmd(TIM1, DISABLE);
    DMA_Cmd(DMA1_Channel5, DISABLE);
    DMA_ClearITPendingBit(DMA1_IT_GL5);
    DMA_SetCurrDataCounter(DMA1_Channel5, BSP_WS2812_BUFFER_LENGTH);
    DMA1_Channel5->MADDR = (uint32_t)au16Ws2812PwmBuffer;
    TIM_SetCounter(TIM1, 0u);
    TIM_SetCompare1(TIM1, 0u);
    DMA_Cmd(DMA1_Channel5, ENABLE);
    TIM_Cmd(TIM1, ENABLE);

    return E_OK;
}

/**
 * @brief Reports whether the WS2812 DMA buffer is available.
 * @retval 1 The driver is idle.
 * @retval 0 A frame is being transmitted.
 */
uint8_t BspWs2812IsIdle(void)
{
    return u8Ws2812DmaIdle;
}

/**
 * @brief Handles TIM1_CH1 PWM transfer completion on DMA1 channel 5.
 */
void DMA1_Channel5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel5_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TE5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_GL5);
        DMA_Cmd(DMA1_Channel5, DISABLE);
        TIM_Cmd(TIM1, DISABLE);
        TIM_SetCompare1(TIM1, 0u);
        u8Ws2812DmaIdle = 1u;
        return;
    }

    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_GL5);
        DMA_Cmd(DMA1_Channel5, DISABLE);
        TIM_Cmd(TIM1, DISABLE);
        TIM_SetCompare1(TIM1, 0u);
        u8Ws2812DmaIdle = 1u;
    }
}
