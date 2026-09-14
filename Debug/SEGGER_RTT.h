/**
 * @file    SEGGER_RTT.h
 * @brief   调试打印兼容层（非 SEGGER 官方 RTT 实现）
 *******************************************************************************
 * @note    本工程没有引入 SEGGER RTT 源码（无 SEGGER_RTT.c / SEGGER_RTT_Conf.h），
 *          但上层代码（user_global.h 等）习惯性包含本头文件。
 *          这里提供一个最小兼容实现：把 SEGGER_RTT_printf() 转接到
 *          WCH 调试串口的 printf（USART1，波特率由 USART_Printf_Init 设定）。
 *
 * @warning 本实现是阻塞式串口输出，仅用于调试日志，禁止在中断或高频任务中调用。
 *          真正的 SEGGER RTT（SWD 内存通道）可后续替换本文件，接口保持不变。
 *******************************************************************************
 */

#ifndef __SEGGER_RTT_H__
#define __SEGGER_RTT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/** @brief RTT 通道号（兼容 SEGGER 语义，本实现忽略该参数） */
#define SEGGER_RTT_CHANNEL_0    (0)

/**
 * @brief  调试打印（兼容 SEGGER_RTT_printf 调用形式）
 * @param[in] BufferIndex  通道号，本实现忽略
 * @param[in] sFormat      格式字符串
 * @param[in] ...          可变参数
 * @return 实际写入的字符数
 * @note   转接到标准 printf()，由 Debug/debug.c 的 _write() 重定向到 USART1。
 */
#define SEGGER_RTT_printf(BufferIndex, ...)   printf(__VA_ARGS__)

/**
 * @brief  调试打印（无格式串版本，兼容接口）
 */
#define SEGGER_RTT_WriteString(BufferIndex, s)   printf("%s", (s))

#ifdef __cplusplus
}
#endif

#endif /* __SEGGER_RTT_H__ */
