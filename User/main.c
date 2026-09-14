/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Brief              : pd-spoofing (CH32X035G8U6) 主程序
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/**
 * @file    main.c
 * @brief   应用入口：初始化各层驱动，然后用 Protothread 时间片调度四个任务
 *******************************************************************************
 * @note    时间片机制（见 Code/Task.h）：
 *            - TIM3 每 1ms 进一次中断，递减 PT_TICK[] 数组（见 Code/UserBsp/bsp_tick.c）；
 *            - 主循环里用 PT_TASK_REG(Rank, Func) 轮询四个任务，
 *              某任务的倒计时归零时才调用其函数，函数内 PT_WAIT_UNTIL 返回下次等待时间。
 *
 *          任务分配：
 *            Rank 0 : UsrDisplayTask —— 屏幕显示（10ms 周期）
 *            Rank 1 : UsrButtonTask  —— 按键扫描（5ms 周期）
 *            Rank 2 : UsrSystemTask  —— 系统状态机（5ms 周期）
 *            Rank 3 : UsrTimeTask    —— 系统计时（10ms 周期）
 *
 *          分层结构：
 *            UserApp/  应用层：任务、状态机、界面逻辑
 *            UserBsp/  板级驱动层：LCD/SPI/按键/ADC
 *            UserDrv/  底层驱动层：节拍、板级 GPIO、外设底层
 *******************************************************************************
 */

#include "main.h"
#include "Task.h"

#include "user_display.h"
#include "user_button.h"
#include "user_system.h"
#include "user_time.h"

#include "bsp_tick.h"
#include "bsp_board.h"
#include "bsp_spi.h"

/**
 * @brief  系统初始化
 * @note   顺序要求：
 *           1) 时钟与延时基础（SystemCoreClockUpdate / Delay_Init）
 *           2) 串口打印（可选，用于调试日志）
 *           3) 板级 GPIO（含 LCD 控制线与按键、输出使能的安全默认电平）
 *           4) 1ms 时间片节拍（TIM3）—— 必须在任何依赖 BspTickGetMs 的驱动之前
 *           5) GPIO 模拟 SPI（LCD 输出通道）
 */
static void SystemInit_User(void)
{
    /* 1) 时钟与延时 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    /* 2) 调试串口（USART1，TX=PB10，对应 LOG-TX） */
    USART_Printf_Init(115200);
    printf("\r\n[BOOT] pd-spoofing start\r\n");
    printf("[BOOT] SYSCLK=%lu Hz ChipID=%08lx\r\n",
           (unsigned long)SystemCoreClock, (unsigned long)DBGMCU_GetCHIPID());

    /* 3) 板级 GPIO：输出使能默认关断，避免上电即带载 */
    BspBoardInit();
    printf("[GPIO] LCD RES=PA1 DC=PA2 CS=PA3 SCK=PA5 SDA=PA7\r\n");
    printf("[GPIO] ADC VOUT=PA0 IBUS=PA4 VBUS=PC0\r\n");
    printf("[GPIO] KEY1=PB3 KEY2=PB4 KEY3=PB6 raw=%u%u%u\r\n",
           (unsigned int)GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN),
           (unsigned int)GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN),
           (unsigned int)GPIO_ReadInputDataBit(KEY3_PORT, KEY3_PIN));

    /* 4) 1ms 时间片节拍（TIM3） */
    BspTickInit();
    printf("[TICK] TIM3 1ms started\r\n");

    /* 5) LCD 的 GPIO 模拟 SPI 接口 */
    BspSpiInit();
    printf("[BOOT] scheduler start\r\n");
}

/*********************************************************************
 * @fn      main
 *
 * @brief   主程序：Protothread 时间片任务调度
 *
 * @return  none
 */
int main(void)
{
    SystemInit_User();

    while (1)
    {
        /* 显示任务：LCD 数据面板刷新 */
        PT_TASK_REG(0, UsrDisplayTask);

        /* 按键任务：3 键事件扫描（单击/双击/长按） */
        PT_TASK_REG(1, UsrButtonTask);

        /* 系统任务：状态机推进（INIT -> POWER_ON -> RUNNING） */
        PT_TASK_REG(2, UsrSystemTask);

        /* 时间任务：上电时间与开机时间累计 */
        PT_TASK_REG(3, UsrTimeTask);
    }
}
