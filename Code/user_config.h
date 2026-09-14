/**
 * @file user_config.h
 * @brief   全局配置参数头文件
 * @note    用户可在此文件中定义项目级配置宏（如控制参数、功能开关等）
 *******************************************************************************
 */

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

/** @brief Enable a static level test on every MCU-controlled LCD signal. */
#define LCD_IO_STATIC_TEST_ENABLE (0u)

/** @brief Static LCD test level: 0 drives low and 1 drives high. */
#define LCD_IO_STATIC_TEST_LEVEL (0u)

/**
 * @brief 是否启用 LCD 显示子系统。
 * @note  置 1（默认）：显示任务注册进调度器，屏幕正常刷新。
 *              此时 LCD 的所有输出都走 DMA 分段异步：整屏填充约 15 次
 *              服务、字符串按块输出，单次调用只占用 CPU 约 1ms，不再
 *              挤占 USB-PD 的 500ms 应答窗口。
 *        置 0：完全不初始化 ST7789V、不注册显示任务，用于单独验证 PD。
 */
#define USER_LCD_ENABLE (1u)

/** @brief Requested fixed PDO index. PDO 1 is the safe default 5 V profile. */
#define USER_PD_REQUEST_PDO_INDEX (1u)

/** @brief Enables USB Power Delivery sink negotiation. */
#define USER_PD_ENABLE (1u)

#endif /* __USER_CONFIG_H__ */
