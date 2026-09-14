/**
 * @file bsp_usb_pd.c
 * @brief Implements a USB Power Delivery sink using the CH32X035 USBPD PHY.
 * @details The protocol flow is adapted from the WCH USBPD_SNK example. The
 *          existing TIM3 scheduler supplies time instead of the example's
 *          dedicated TIM1 millisecond interrupt.
 */

#include "bsp_usb_pd.h"
#include "ch32x035_gpio.h"
#include "ch32x035_misc.h"
#include "ch32x035_rcc.h"
#include "ch32x035_usbpd.h"

static uint8_t au8PdRxBuffer[BSP_USB_PD_BUFFER_SIZE] __attribute__((aligned(4)));
static uint8_t au8PdTxBuffer[BSP_USB_PD_BUFFER_SIZE] __attribute__((aligned(4)));
static uint8_t au8PdAckBuffer[2];
static uint8_t au8PdSourceCapabilities[BSP_USB_PD_SOURCE_CAP_SIZE];
static const uint8_t au8PdSinkCapability[4] = {0x64u, 0x90u, 0x01u, 0x36u};

static volatile uint8_t u8PdMessageReceived = 0u;
static volatile uint8_t u8PdHardResetReceived = 0u;
static volatile uint32_t u32PdRxInterruptCount = 0u;
static CC_STATUS ePdState = STA_IDLE;
static uint8_t u8PdMessageId = 0u;
static uint8_t u8PdDetectCount = 0u;
static uint16_t u16PdDetectTimerMs = 0u;
static uint16_t u16PdCommunicationTimerMs = 0u;
static tBspUsbPdStatusDef tBspUsbPdStatus;

static void BspUsbPdEnterReceiveMode(void);
static void BspUsbPdSinkInit(void);
static void BspUsbPdPhyReset(void);
static void BspUsbPdSendPhy(uint8_t u8Wait, uint8_t *pu8Buffer,
                            uint8_t u8Length, uint8_t u8Sop);

/**
 * @brief Selects sink mode and enables the external CC pull-down state.
 */
static void BspUsbPdSinkInit(void)
{
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;
}

/**
 * @brief Places the USBPD peripheral in BMC receive mode.
 */
static void BspUsbPdEnterReceiveMode(void)
{
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;
    USBPD->CONFIG |= IE_RX_ACT | IE_RX_RESET | PD_DMA_EN;
    USBPD->DMA = (uint32_t)au8PdRxBuffer;
    USBPD->CONTROL &= ~PD_TX_EN;
    USBPD->BMC_CLK_CNT = UPD_TMR_RX_48M;
    USBPD->CONTROL |= BMC_START;
    NVIC_EnableIRQ(USBPD_IRQn);
}

/**
 * @brief Resets protocol state while retaining physical sink configuration.
 */
static void BspUsbPdPhyReset(void)
{
    BspUsbPdSinkInit();
    ePdState = STA_IDLE;
    u8PdMessageId = 0u;
    u16PdCommunicationTimerMs = 0u;
    u8PdMessageReceived = 0u;
    tBspUsbPdStatus.u8ContractValid = 0u;
}

/**
 * @brief Starts one raw BMC packet transmission.
 * @param[in] u8Wait Nonzero waits until the PHY finishes transmitting.
 * @param[in] pu8Buffer Pointer to the packet bytes, or null for reset symbols.
 * @param[in] u8Length Number of packet bytes.
 * @param[in] u8Sop SOP selector supported by the CH32X035 peripheral.
 */
static void BspUsbPdSendPhy(uint8_t u8Wait, uint8_t *pu8Buffer,
                            uint8_t u8Length, uint8_t u8Sop)
{
    if ((USBPD->CONFIG & CC_SEL) != 0u)
    {
        USBPD->PORT_CC2 |= CC_LVE;
    }
    else
    {
        USBPD->PORT_CC1 |= CC_LVE;
    }

    USBPD->BMC_CLK_CNT = UPD_TMR_TX_48M;
    USBPD->DMA = (uint32_t)pu8Buffer;
    USBPD->TX_SEL = u8Sop;
    USBPD->BMC_TX_SZ = u8Length;
    USBPD->CONTROL |= PD_TX_EN;
    USBPD->STATUS &= BMC_AUX_INVALID;
    USBPD->CONTROL |= BMC_START;

    if (u8Wait != 0u)
    {
        while ((USBPD->STATUS & IF_TX_END) == 0u)
        {
        }
        USBPD->STATUS |= IF_TX_END;
        USBPD->PORT_CC1 &= ~CC_LVE;
        USBPD->PORT_CC2 &= ~CC_LVE;

        /* Match the WCH USBPD_SNK flow: switch to RX immediately so the
         * caller can poll the GoodCRC response for the packet just sent. */
        USBPD->CONFIG |= PD_ALL_CLR;
        USBPD->CONFIG &= ~PD_ALL_CLR;
        USBPD->CONTROL &= ~PD_TX_EN;
        USBPD->DMA = (uint32_t)au8PdRxBuffer;
        USBPD->BMC_CLK_CNT = UPD_TMR_RX_48M;
        USBPD->CONTROL |= BMC_START;
    }
}

/**
 * @brief Loads a sink/UFP PD message header into the transmit buffer.
 * @param[in] u8Extended Nonzero sets the extended-message flag.
 * @param[in] u8MessageType USB-PD message type.
 */
static void BspUsbPdLoadHeader(uint8_t u8Extended, uint8_t u8MessageType)
{
    au8PdTxBuffer[0] = (uint8_t)(u8MessageType | 0x40u); /* PD 2.0, UFP. */
    au8PdTxBuffer[1] = (uint8_t)(u8PdMessageId & 0x0Eu); /* Sink power role. */
    if (u8Extended != 0u)
    {
        au8PdTxBuffer[1] |= 0x80u;
    }
}

/**
 * @brief Sends one PD message and waits for its GoodCRC response.
 * @param[in] pu8Payload Pointer to the data objects, or null for a control message.
 * @param[in] u8Length Payload length; it must be a multiple of four up to 28 bytes.
 * @retval E_OK The source acknowledged the message.
 * @retval E_ERROR The payload was invalid or all retries timed out.
 */
static eStatusDef BspUsbPdSendMessage(const uint8_t *pu8Payload, uint8_t u8Length)
{
    uint8_t Attempt;
    uint8_t ByteIndex;
    uint16_t Timeout;

    if (((u8Length & 0x03u) != 0u) || (u8Length > 28u))
    {
        return E_ERROR;
    }

    au8PdTxBuffer[1] |= (uint8_t)((u8Length >> 2u) << 4u);
    for (ByteIndex = 0u; ByteIndex < u8Length; ByteIndex++)
    {
        au8PdTxBuffer[2u + ByteIndex] = pu8Payload[ByteIndex];
    }

    for (Attempt = 0u; Attempt < BSP_USB_PD_TX_RETRY_COUNT; Attempt++)
    {
        NVIC_DisableIRQ(USBPD_IRQn);
        BspUsbPdSendPhy(1u, au8PdTxBuffer, (uint8_t)(u8Length + 2u), UPD_SOP0);

        for (Timeout = 0u; Timeout < 250u; Timeout++)
        {
            if ((USBPD->STATUS & IF_RX_ACT) != 0u)
            {
                USBPD->STATUS |= IF_RX_ACT;
                if ((USBPD->BMC_BYTE_CNT == 6u) &&
                    ((au8PdRxBuffer[0] & 0x1Fu) == DEF_TYPE_GOODCRC))
                {
                    u8PdMessageId = (uint8_t)(u8PdMessageId + 2u);
                    BspUsbPdEnterReceiveMode();
                    return E_OK;
                }
            }
            Delay_Us(3u);
        }
    }

    BspUsbPdEnterReceiveMode();
    return E_ERROR;
}

/**
 * @brief Decodes a fixed supply PDO.
 * @param[in] pu8Pdo Pointer to four little-endian PDO bytes.
 * @param[out] pu16CurrentMa Pointer receiving maximum current in milliamperes.
 * @param[out] pu16VoltageMv Pointer receiving voltage in millivolts.
 */
static void BspUsbPdDecodeFixedPdo(const uint8_t *pu8Pdo,
                                   uint16_t *pu16CurrentMa,
                                   uint16_t *pu16VoltageMv)
{
    uint32_t Pdo;

    Pdo = (uint32_t)pu8Pdo[0] |
          ((uint32_t)pu8Pdo[1] << 8u) |
          ((uint32_t)pu8Pdo[2] << 16u) |
          ((uint32_t)pu8Pdo[3] << 24u);
    *pu16CurrentMa = (uint16_t)((Pdo & 0x03FFu) * 10u);
    *pu16VoltageMv = (uint16_t)(((Pdo >> 10u) & 0x03FFu) * 50u);
}

/**
 * @brief Saves fixed PDOs from the most recent Source_Capabilities message.
 */
static void BspUsbPdSaveSourceCapabilities(void)
{
    uint8_t AdvertisedCount;
    uint8_t FixedCount;
    uint8_t Index;

    AdvertisedCount = (uint8_t)((au8PdRxBuffer[1] >> 4u) & 0x07u);
    FixedCount = 0u;
    for (Index = 0u; Index < AdvertisedCount; Index++)
    {
        if ((au8PdRxBuffer[2u + (Index * 4u) + 3u] & 0xC0u) != 0u)
        {
            break;
        }
        FixedCount++;
    }

    tBspUsbPdStatus.u8PdoCount = FixedCount;
    au8PdSourceCapabilities[0] = FixedCount;
    memcpy(&au8PdSourceCapabilities[1], &au8PdRxBuffer[2],
           (size_t)FixedCount * 4u);
}

/**
 * @brief Requests the configured fixed PDO from the attached source.
 */
static void BspUsbPdStartPdoRequest(uint8_t u8RequestedIndex)
{
    const uint8_t *pu8Pdo;
    uint16_t CurrentMa;
    uint16_t VoltageMv;
    uint16_t CurrentUnits;
    uint32_t RequestDataObject;
    uint8_t au8Request[4];

    pu8Pdo = &au8PdSourceCapabilities[1u + ((u8RequestedIndex - 1u) * 4u)];
    BspUsbPdDecodeFixedPdo(pu8Pdo, &CurrentMa, &VoltageMv);
    CurrentUnits = (uint16_t)(CurrentMa / 10u);
    RequestDataObject = ((uint32_t)u8RequestedIndex << 28u) |
                        0x03000000u |
                        ((uint32_t)CurrentUnits << 10u) |
                        CurrentUnits;
    au8Request[0] = (uint8_t)RequestDataObject;
    au8Request[1] = (uint8_t)(RequestDataObject >> 8u);
    au8Request[2] = (uint8_t)(RequestDataObject >> 16u);
    au8Request[3] = (uint8_t)(RequestDataObject >> 24u);

    tBspUsbPdStatus.u8RequestedPdo = u8RequestedIndex;
    tBspUsbPdStatus.u16CurrentMa = CurrentMa;
    tBspUsbPdStatus.u16VoltageMv = VoltageMv;
    printf("[PD] request PDO%u: %u mV %u mA\r\n",
           (unsigned int)u8RequestedIndex,
           (unsigned int)VoltageMv,
           (unsigned int)CurrentMa);

    BspUsbPdLoadHeader(0u, DEF_TYPE_REQUEST);
    if (BspUsbPdSendMessage(au8Request, sizeof(au8Request)) == E_OK)
    {
        ePdState = STA_RX_ACCEPT_WAIT;
    }
    else
    {
        ePdState = STA_TX_SOFTRST;
        printf("[PD] request GoodCRC timeout\r\n");
    }
    u16PdCommunicationTimerMs = 0u;
}

/**
 * @brief Detects which CC input contains a source pull-up.
 * @retval 0 No source was detected.
 * @retval 1 A source was detected on CC1.
 * @retval 2 A source was detected on CC2.
 */
static uint8_t BspUsbPdDetectCc(void)
{
    uint8_t Cc1Detected;
    uint8_t Cc2Detected;

    USBPD->PORT_CC1 &= ~(CC_CMP_Mask | PA_CC_AI);
    USBPD->PORT_CC1 |= CC_CMP_22;
    Delay_Us(2u);
    Cc1Detected = ((USBPD->PORT_CC1 & PA_CC_AI) != 0u) ? 1u : 0u;

    USBPD->PORT_CC2 &= ~(CC_CMP_Mask | PA_CC_AI);
    USBPD->PORT_CC2 |= CC_CMP_22;
    Delay_Us(2u);
    Cc2Detected = ((USBPD->PORT_CC2 & PA_CC_AI) != 0u) ? 1u : 0u;

    if (Cc1Detected != 0u)
    {
        return 1u;
    }
    if (Cc2Detected != 0u)
    {
        return 2u;
    }
    return 0u;
}

/**
 * @brief Processes stable CC source attachment.
 */
static void BspUsbPdProcessDetection(void)
{
    uint8_t CcLine;

    if (tBspUsbPdStatus.u8Connected != 0u)
    {
        return;
    }

    CcLine = BspUsbPdDetectCc();
    if (CcLine == 0u)
    {
        u8PdDetectCount = 0u;
        return;
    }

    u8PdDetectCount++;
    if (u8PdDetectCount < BSP_USB_PD_DETECT_STABLE_COUNT)
    {
        return;
    }

    u8PdDetectCount = 0u;
    tBspUsbPdStatus.u8Connected = 1u;
    tBspUsbPdStatus.u8CcLine = CcLine;
    if (CcLine == 1u)
    {
        USBPD->CONFIG &= ~CC_SEL;
    }
    else
    {
        USBPD->CONFIG |= CC_SEL;
    }

    /* CC detection changes comparator and channel-selection state. Restart the
     * BMC receiver so it listens on the selected wire from a clean state. */
    BspUsbPdEnterReceiveMode();
    ePdState = STA_SRC_CONNECT;
    u16PdCommunicationTimerMs = 0u;
    printf("[PD] source connected on CC%u\r\n", (unsigned int)CcLine);
}

/**
 * @brief Processes a received source message after its GoodCRC was transmitted.
 */
static void BspUsbPdProcessReceivedMessage(void)
{
    uint8_t MessageType;
    uint8_t Index;
    uint16_t CurrentMa;
    uint16_t VoltageMv;

    u8PdMessageReceived = 0u;
    MessageType = (uint8_t)(au8PdRxBuffer[0] & 0x1Fu);
    switch (MessageType)
    {
    case DEF_TYPE_SRC_CAP:
        Delay_Ms(5u);
        BspUsbPdSaveSourceCapabilities();
        printf("[PD] source capabilities: %u fixed PDO(s)\r\n",
               (unsigned int)tBspUsbPdStatus.u8PdoCount);
        for (Index = 0u; Index < tBspUsbPdStatus.u8PdoCount; Index++)
        {
            BspUsbPdDecodeFixedPdo(&au8PdSourceCapabilities[1u + (Index * 4u)],
                                   &CurrentMa, &VoltageMv);
            printf("[PD] PDO%u: %u mV %u mA\r\n",
                   (unsigned int)(Index + 1u),
                   (unsigned int)VoltageMv,
                   (unsigned int)CurrentMa);
        }
        if (tBspUsbPdStatus.u8PdoCount != 0u)
        {
            Index = USER_PD_REQUEST_PDO_INDEX;
            if ((Index == 0u) || (Index > tBspUsbPdStatus.u8PdoCount))
            {
                Index = 1u;
            }
            BspUsbPdStartPdoRequest(Index);
        }
        break;

    case DEF_TYPE_ACCEPT:
        ePdState = STA_RX_PS_RDY_WAIT;
        u16PdCommunicationTimerMs = 0u;
        printf("[PD] request accepted\r\n");
        break;

    case DEF_TYPE_PS_RDY:
        ePdState = STA_IDLE;
        tBspUsbPdStatus.u8ContractValid = 1u;
        printf("[PD] contract ready: %u mV %u mA\r\n",
               (unsigned int)tBspUsbPdStatus.u16VoltageMv,
               (unsigned int)tBspUsbPdStatus.u16CurrentMa);
        break;

    case DEF_TYPE_GET_SNK_CAP:
        Delay_Ms(1u);
        BspUsbPdLoadHeader(0u, DEF_TYPE_SNK_CAP);
        (void)BspUsbPdSendMessage(au8PdSinkCapability,
                                  sizeof(au8PdSinkCapability));
        break;

    case DEF_TYPE_SOFT_RESET:
        Delay_Ms(1u);
        BspUsbPdLoadHeader(0u, DEF_TYPE_ACCEPT);
        (void)BspUsbPdSendMessage(NULL, 0u);
        break;

    case DEF_TYPE_VCONN_SWAP:
    case DEF_TYPE_PR_SWAP:
    case DEF_TYPE_DR_SWAP:
        Delay_Ms(1u);
        BspUsbPdLoadHeader(0u, DEF_TYPE_REJECT);
        (void)BspUsbPdSendMessage(NULL, 0u);
        break;

    case DEF_TYPE_WAIT:
        break;

    default:
        printf("[PD] unsupported message type 0x%02x\r\n",
               (unsigned int)MessageType);
        break;
    }

    BspUsbPdEnterReceiveMode();
}

/**
 * @brief Initializes PC14/PC15 and the CH32X035 USBPD sink peripheral.
 */
void BspUsbPdInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));
    memset(&NVIC_InitStructure, 0, sizeof(NVIC_InitStructure));
    memset(au8PdRxBuffer, 0, sizeof(au8PdRxBuffer));
    memset(au8PdTxBuffer, 0, sizeof(au8PdTxBuffer));
    memset(au8PdSourceCapabilities, 0, sizeof(au8PdSourceCapabilities));
    memset(&tBspUsbPdStatus, 0, sizeof(tBspUsbPdStatus));

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBPD, ENABLE);

    GPIO_InitStructure.GPIO_Pin = USB_PD_CC1_PIN | USB_PD_CC2_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(USB_PD_CC_PORT, &GPIO_InitStructure);

    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;
    USBPD->CONFIG = PD_DMA_EN;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE |
                    IF_RX_ACT | IF_RX_RESET | IF_TX_END;

    NVIC_InitStructure.NVIC_IRQChannel = USBPD_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0u;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1u;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    u8PdDetectCount = 0u;
    u16PdDetectTimerMs = 0u;
    u8PdHardResetReceived = 0u;
    u32PdRxInterruptCount = 0u;
    BspUsbPdPhyReset();
    BspUsbPdEnterReceiveMode();
    printf("[PD] sink initialized: CC1=PC14 CC2=PC15 request=PDO%u\r\n",
           (unsigned int)USER_PD_REQUEST_PDO_INDEX);
}

/**
 * @brief Advances CC detection and the USB-PD protocol state machine.
 * @param[in] u16ElapsedMs Milliseconds since the previous call.
 */
void BspUsbPdProcess(uint16_t u16ElapsedMs)
{
    if (u8PdHardResetReceived != 0u)
    {
        u8PdHardResetReceived = 0u;
        BspUsbPdPhyReset();
        tBspUsbPdStatus.u8Connected = 0u;
        tBspUsbPdStatus.u8CcLine = 0u;
        BspUsbPdEnterReceiveMode();
        printf("[PD] hard reset received\r\n");
    }

    u16PdDetectTimerMs = (uint16_t)(u16PdDetectTimerMs + u16ElapsedMs);
    if (u16PdDetectTimerMs >= BSP_USB_PD_DETECT_INTERVAL_MS)
    {
        u16PdDetectTimerMs = 0u;
        BspUsbPdProcessDetection();
    }

    switch (ePdState)
    {
    case STA_SRC_CONNECT:
        u16PdCommunicationTimerMs =
            (uint16_t)(u16PdCommunicationTimerMs + u16ElapsedMs);
        if (u16PdCommunicationTimerMs >= BSP_USB_PD_SOURCE_CAP_TIMEOUT_MS)
        {
            printf("[PD] source capabilities timeout: cc=%u rx=%lu cfg=%08lx ctl=%08lx stat=%08lx cnt=%lu\r\n",
                   (unsigned int)tBspUsbPdStatus.u8CcLine,
                   (unsigned long)u32PdRxInterruptCount,
                   (unsigned long)USBPD->CONFIG,
                   (unsigned long)USBPD->CONTROL,
                   (unsigned long)USBPD->STATUS,
                   (unsigned long)USBPD->BMC_BYTE_CNT);
            u16PdCommunicationTimerMs = 0u;
            BspUsbPdPhyReset();
            ePdState = STA_SRC_CONNECT;
            BspUsbPdEnterReceiveMode();
            printf("[PD] source capabilities timeout, retry\r\n");
        }
        break;

    case STA_RX_ACCEPT_WAIT:
    case STA_RX_PS_RDY_WAIT:
        u16PdCommunicationTimerMs =
            (uint16_t)(u16PdCommunicationTimerMs + u16ElapsedMs);
        if (u16PdCommunicationTimerMs >= BSP_USB_PD_RESPONSE_TIMEOUT_MS)
        {
            ePdState = STA_TX_SOFTRST;
            u16PdCommunicationTimerMs = 0u;
        }
        break;

    case STA_TX_SOFTRST:
        BspUsbPdLoadHeader(0u, DEF_TYPE_SOFT_RESET);
        if (BspUsbPdSendMessage(NULL, 0u) == E_OK)
        {
            ePdState = STA_SRC_CONNECT;
        }
        else
        {
            ePdState = STA_TX_HRST;
        }
        u16PdCommunicationTimerMs = 0u;
        break;

    case STA_TX_HRST:
        NVIC_DisableIRQ(USBPD_IRQn);
        BspUsbPdSendPhy(1u, NULL, 0u, UPD_HARD_RESET);
        BspUsbPdPhyReset();
        ePdState = STA_SRC_CONNECT;
        BspUsbPdEnterReceiveMode();
        break;

    default:
        break;
    }

    if (u8PdMessageReceived != 0u)
    {
        BspUsbPdProcessReceivedMessage();
    }
}

/**
 * @brief Returns the most recent USB-PD connection and contract information.
 * @return Read-only pointer to driver status.
 */
const tBspUsbPdStatusDef *BspUsbPdGetStatus(void)
{
    return &tBspUsbPdStatus;
}

/**
 * @brief Starts renegotiation for one advertised fixed PDO.
 * @param[in] u8PdoIndex One-based fixed PDO index.
 * @retval E_OK The request message was started.
 * @retval E_BUSY A previous negotiation is still active.
 * @retval E_ERROR No source is connected or the index is unavailable.
 */
eStatusDef BspUsbPdRequestPdo(uint8_t u8PdoIndex)
{
    if ((tBspUsbPdStatus.u8Connected == 0u) ||
        (u8PdoIndex == 0u) ||
        (u8PdoIndex > tBspUsbPdStatus.u8PdoCount))
    {
        return E_ERROR;
    }

    if (ePdState != STA_IDLE)
    {
        return E_BUSY;
    }

    tBspUsbPdStatus.u8ContractValid = 0u;
    BspUsbPdStartPdoRequest(u8PdoIndex);
    return E_OK;
}

/**
 * @brief Handles received USB-PD messages and PHY reset events.
 */
void USBPD_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBPD_IRQHandler(void)
{
    if ((USBPD->STATUS & IF_RX_ACT) != 0u)
    {
        u32PdRxInterruptCount++;
        USBPD->STATUS |= IF_RX_ACT;
        if (((USBPD->STATUS & MASK_PD_STAT) == PD_RX_SOP0) &&
            (USBPD->BMC_BYTE_CNT >= 6u) &&
            ((USBPD->BMC_BYTE_CNT != 6u) ||
             ((au8PdRxBuffer[0] & 0x1Fu) != DEF_TYPE_GOODCRC)))
        {
            Delay_Us(30u);
            au8PdAckBuffer[0] = 0x41u;
            au8PdAckBuffer[1] = (uint8_t)(au8PdRxBuffer[1] & 0x0Eu);
            USBPD->CONFIG |= IE_TX_END;
            BspUsbPdSendPhy(0u, au8PdAckBuffer, 2u, UPD_SOP0);
        }
    }

    if ((USBPD->STATUS & IF_TX_END) != 0u)
    {
        USBPD->PORT_CC1 &= ~CC_LVE;
        USBPD->PORT_CC2 &= ~CC_LVE;
        NVIC_DisableIRQ(USBPD_IRQn);
        u8PdMessageReceived = 1u;
        USBPD->STATUS |= IF_TX_END;
    }

    if ((USBPD->STATUS & IF_RX_RESET) != 0u)
    {
        USBPD->STATUS |= IF_RX_RESET;
        NVIC_DisableIRQ(USBPD_IRQn);
        u8PdHardResetReceived = 1u;
    }
}
