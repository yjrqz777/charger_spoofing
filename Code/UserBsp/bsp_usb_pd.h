/**
 * @file bsp_usb_pd.h
 * @brief Declares the CH32X035 USB Power Delivery sink driver.
 */

#ifndef __BSP_USB_PD_H__
#define __BSP_USB_PD_H__

#include "main.h"

#define BSP_USB_PD_BUFFER_SIZE           (34u)   /* Header, seven PDOs, and CRC. */
#define BSP_USB_PD_SOURCE_CAP_SIZE       (29u)   /* Count byte plus seven PDOs. */
#define BSP_USB_PD_DETECT_INTERVAL_MS    (5u)    /* CC sampling interval. */
#define BSP_USB_PD_DETECT_STABLE_COUNT   (5u)    /* Required consecutive samples. */
#define BSP_USB_PD_SOURCE_CAP_TIMEOUT_MS (1000u) /* Source capability timeout. */
#define BSP_USB_PD_RESPONSE_TIMEOUT_MS   (500u)  /* ACCEPT or PS_RDY timeout. */
#define BSP_USB_PD_TX_RETRY_COUNT        (3u)    /* Number of message attempts. */

/**
 * @brief Describes the current USB Power Delivery contract.
 */
typedef struct tBspUsbPdStatusDef
{
    uint8_t u8Connected;          /**< A source is detected on either CC pin. */
    uint8_t u8ContractValid;      /**< ACCEPT and PS_RDY completed successfully. */
    uint8_t u8CcLine;             /**< Active CC line: zero, one, or two. */
    uint8_t u8PdoCount;           /**< Number of fixed PDOs advertised by the source. */
    uint8_t u8RequestedPdo;       /**< PDO index selected for the request. */
    uint16_t u16VoltageMv;        /**< Requested voltage in millivolts. */
    uint16_t u16CurrentMa;        /**< Requested current in milliamperes. */
} tBspUsbPdStatusDef;

void BspUsbPdInit(void);
void BspUsbPdProcess(uint16_t u16ElapsedMs);
eStatusDef BspUsbPdRequestPdo(uint8_t u8PdoIndex);
const tBspUsbPdStatusDef *BspUsbPdGetStatus(void);

#endif /* __BSP_USB_PD_H__ */
