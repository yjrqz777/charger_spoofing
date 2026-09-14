/**
 * @file    bsp_board.c
 * @brief   板级 GPIO 初始化实现
 *******************************************************************************
 * @note    引脚来源：Code/main.h（依据原理图 doc/SCH_Schematic1_2026-09-14.pdf）
 *          上电默认安全状态：VOUT-EN = 低（功率输出关断）、指示灯熄灭。
 *******************************************************************************
 */

#include "bsp_board.h"

/** @brief VOUT-EN 当前逻辑状态（0=关断, 1=导通） */
static uint8_t s_u8VoutEnable = 0u;

/**
 * @brief  配置一个推挽输出引脚
 * @param[in] GPIOx    端口
 * @param[in] u32Pin   引脚掩码
 */
static void BspBoardConfigOutput(GPIO_TypeDef *GPIOx, uint32_t u32Pin)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));
    GPIO_InitStructure.GPIO_Pin   = u32Pin;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOx, &GPIO_InitStructure);
}

/**
 * @brief  配置一个上拉输入引脚
 * @param[in] GPIOx    端口
 * @param[in] u32Pin   引脚掩码
 */
static void BspBoardConfigInputPullUp(GPIO_TypeDef *GPIOx, uint32_t u32Pin)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));
    GPIO_InitStructure.GPIO_Pin   = u32Pin;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOx, &GPIO_InitStructure);
}

/**
 * @brief  配置一个模拟输入引脚（供 ADC 使用）
 * @param[in] GPIOx    端口
 * @param[in] u32Pin   引脚掩码
 */
static void BspBoardConfigAnalog(GPIO_TypeDef *GPIOx, uint32_t u32Pin)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));
    GPIO_InitStructure.GPIO_Pin   = u32Pin;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOx, &GPIO_InitStructure);
}

void BspBoardInit(void)
{
    /* 1) 使能 GPIOA / GPIOB / GPIOC 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    /* 2) 先输出安全电平，再配置为输出，避免上电瞬间误动作
     *    VOUT-EN 为高有效 -> 置低 = 输出关断
     *    LED 为低电平点亮 -> 置高 = 熄灭 */
    GPIO_ResetBits(VOUT_EN_PORT, VOUT_EN_PIN);
    GPIO_SetBits(LED_RUN_PORT, LED_RUN_PIN);

    /* 3) LCD 控制线：RES / DC / CS 推挽输出，初始 CS=1、DC=1、RES=1（复位释放） */
    GPIO_SetBits(LCD_CS_PORT,  LCD_CS_PIN);
    GPIO_SetBits(LCD_DC_PORT,  LCD_DC_PIN);
    GPIO_SetBits(LCD_RES_PORT, LCD_RES_PIN);

    BspBoardConfigOutput(LCD_RES_PORT, LCD_RES_PIN);
    BspBoardConfigOutput(LCD_DC_PORT,  LCD_DC_PIN);
    BspBoardConfigOutput(LCD_CS_PORT,  LCD_CS_PIN);

    /* 4) VOUT-EN 与指示灯推挽输出（已在 2) 置安全电平） */
    BspBoardConfigOutput(VOUT_EN_PORT, VOUT_EN_PIN);
    BspBoardConfigOutput(LED_RUN_PORT, LED_RUN_PIN);
    s_u8VoutEnable = 0u;

    /* 5) 按键输入：外部已有 10k 上拉 + 10nF 消抖，内部再开上拉提高抗扰度 */
    BspBoardConfigInputPullUp(KEY1_PORT, KEY1_PIN);
    BspBoardConfigInputPullUp(KEY2_PORT, KEY2_PIN);
    BspBoardConfigInputPullUp(KEY3_PORT, KEY3_PIN);

    /* 6) ADC 采样脚配置为模拟输入
     *    注意：必须用 GPIO_Mode_AIN，否则数字输入缓冲会引入漏电与噪声 */
    BspBoardConfigAnalog(ADC_IBUS_PORT, ADC_IBUS_PIN);
    BspBoardConfigAnalog(ADC_VBUS_PORT, ADC_VBUS_PIN);
    BspBoardConfigAnalog(ADC_VOUT_PORT, ADC_VOUT_PIN);
}

void BspBoardSetVoutEnable(uint8_t u8Enable)
{
    if (u8Enable != 0u)
    {
        GPIO_SetBits(VOUT_EN_PORT, VOUT_EN_PIN);
        s_u8VoutEnable = 1u;
    }
    else
    {
        GPIO_ResetBits(VOUT_EN_PORT, VOUT_EN_PIN);
        s_u8VoutEnable = 0u;
    }
}

uint8_t BspBoardGetVoutEnable(void)
{
    return s_u8VoutEnable;
}

void BspBoardSetLed(uint8_t u8On)
{
    /* LED 为低电平点亮：点亮 -> 输出低；熄灭 -> 输出高 */
    if (u8On != 0u)
    {
        GPIO_ResetBits(LED_RUN_PORT, LED_RUN_PIN);
    }
    else
    {
        GPIO_SetBits(LED_RUN_PORT, LED_RUN_PIN);
    }
}
