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

/** @brief Requested fixed PDO index. PDO 1 is the safe default 5 V profile. */
#define USER_PD_REQUEST_PDO_INDEX (1u)

/** @brief Enables USB Power Delivery sink negotiation. */
#define USER_PD_ENABLE (1u)

#endif /* __USER_CONFIG_H__ */
