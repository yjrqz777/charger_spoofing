/**
 * @file bsp_spi.h
 * @brief Declares the GPIO-driven software SPI interface used by the LCD.
 */

#ifndef __BSP_SPI_H__
#define __BSP_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void BspSpiInit(void);
void BspSpiWriteByte(uint8_t u8Data);
void BspSpiWriteBufferBlocking(const uint8_t *pu8Data, uint16_t u16Len);
uint8_t BspSpiHasError(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SPI_H__ */
