/**
 * @file bsp_usb_pd.c
 * @brief USB Power Delivery sink driver for the CH32X035 USBPD PHY.
 * @details 本文件是 tools/EVT/EXAM/USBPD/USBPD_SNK/User/PD_Process.c 的直接移植。
 *          与原例程的对应关系：
 *
 *            例程 PD_Rx_Mode()             <-> BspUsbPdRxMode()
 *            例程 PD_SINK_Init()           <-> BspUsbPdSinkInit()
 *            例程 PD_PHY_Reset()           <-> BspUsbPdPhyReset()
 *            例程 PD_Init()                <-> BspUsbPdInit()
 *            例程 PD_Detect()              <-> BspUsbPdDetectCc()
 *            例程 PD_Det_Proc()            <-> BspUsbPdDetectProc()
 *            例程 PD_Phy_SendPack()        <-> BspUsbPdSendPhy()
 *            例程 PD_Load_Header()         <-> BspUsbPdLoadHeader()
 *            例程 PD_Send_Handle()         <-> BspUsbPdSendMessage()
 *            例程 PDO_Request()            <-> BspUsbPdStartPdoRequest()
 *            例程 PD_Save_Adapter_SrcCap() <-> BspUsbPdSaveSourceCapabilities()
 *            例程 PD_PDO_Analyse()         <-> BspUsbPdDecodeFixedPdo()
 *            例程 PD_Main_Proc()           <-> BspUsbPdProcess()
 *
 *          与原例程仅有的结构性差别（协议行为保持一致）：
 *            1) 时基来自本工程 TIM3 的 1ms 节拍，而不是例程的 TIM1 中断；
 *            2) 例程在 main() 的 while(1) 里连续调用 PD_Main_Proc()，
 *               本工程由 Protothread 任务每 1ms 调用一次；
 *            3) 例程收到 PS_RDY 后不再做任何事，本工程额外置位契约状态，
 *               供按键切换档位与显示使用。
 *
 * @warning 版本标识见 BSP_USB_PD_VERSION_STR。串口日志里若没有
 *          "[PD] driver v2-example-port" 这一行，说明烧的不是本文件
 *          （IDE 覆盖了改动，或者根本没有重新编译）。
 */

#include "bsp_usb_pd.h"
#include "ch32x035_gpio.h"
#include "ch32x035_misc.h"
#include "ch32x035_rcc.h"
#include "ch32x035_usbpd.h"

/** @brief 构建标识：用来确认烧录的固件确实是本文件编译出来的。 */
#define BSP_USB_PD_VERSION_STR "v2-example-port"

/* ========================================================================== *
 *  与例程一致的状态数据
 * ========================================================================== */

/** @brief 例程 PD_Rx_Buf[34] —— PD 接收缓冲 */
static uint8_t au8PdRxBuffer[34] __attribute__((aligned(4)));

/** @brief 例程 PD_Tx_Buf[34] —— PD 发送缓冲 */
static uint8_t au8PdTxBuffer[34] __attribute__((aligned(4)));

/** @brief 例程 PD_Ack_Buf[2] —— GoodCRC 应答缓冲 */
static uint8_t au8PdAckBuffer[2];

/** @brief 例程 Adapter_SrcCap[30] —— 电源送来的 SrcCap（首字节为固定档位数） */
static uint8_t au8PdSourceCapabilities[30];

/** @brief 例程 SinkCap_5V1A_Tab[4] —— 本受电端对外声明的能力 */
static const uint8_t au8PdSinkCapability[4] = {0x64u, 0x90u, 0x01u, 0x36u};

/** @brief 例程 PD_Ctl.Msg_ID —— 消息 ID，左对齐存放（bit[3:1]），故按 +2 递增 */
static uint8_t u8PdMessageId = 0u;

/** @brief 例程 PD_Ctl.PD_State */
static CC_STATUS ePdState = STA_IDLE;

/** @brief 例程 PD_Ctl.Flag.Bit.Msg_Recvd —— 已收报文待主循环处理 */
static volatile uint8_t u8PdMessageReceived = 0u;

/** @brief 例程 PD_Ctl.PD_Comm_Timer —— 当前状态的通信超时计数（ms） */
static uint16_t u16PdCommunicationTimerMs = 0u;

/** @brief 例程 PD_Ctl.Det_Timer / Det_Cnt —— 4ms 检测节拍与连续命中计数 */
static uint8_t u8PdDetectTimerMs = 0u;
static uint8_t u8PdDetectCount = 0u;

/** @brief 例程 PD_Ctl.Err_Op_Cnt —— SrcCap 等待重试次数 */
static uint8_t u8PdErrOpCount = 0u;

/** @brief 例程 Tmr_Ms_Dlt —— 两次 PD_Main_Proc 之间的毫秒增量 */
static volatile uint8_t u8PdMsDelta = 0u;

/** @brief 对外暴露的连接/契约状态（供显示与按键使用，例程中无此结构） */
static tBspUsbPdStatusDef tBspUsbPdStatus;

/** @brief 诊断计数：已成功发出的报文数 / 已接收处理完的报文数 */
static uint16_t u16PdTxCount = 0u;
static uint16_t u16PdRxCount = 0u;

/* ========================================================================== *
 *  内部函数声明（顺序与例程一致）
 * ========================================================================== */

static void BspUsbPdRxMode(void);
static void BspUsbPdSinkInit(void);
static void BspUsbPdPhyReset(void);
static void BspUsbPdDetectProc(void);
static uint8_t BspUsbPdDetectCc(void);
static void BspUsbPdSendPhy(uint8_t u8Mode, uint8_t *pu8Buffer,
                            uint8_t u8Length, uint8_t u8Sop);
static void BspUsbPdLoadHeader(uint8_t u8Extended, uint8_t u8MessageType);
static eStatusDef BspUsbPdSendMessage(uint8_t *pu8Payload, uint8_t u8Length);
static void BspUsbPdStartPdoRequest(uint8_t u8PdoIndex);
static void BspUsbPdSaveSourceCapabilities(void);
static void BspUsbPdDecodeFixedPdo(uint8_t u8PdoIndex, const uint8_t *pu8SourceCap,
                                   uint16_t *pu16CurrentMa, uint16_t *pu16VoltageMv);

/**
 * @brief 对应例程 PD_Rx_Mode()：进入 BMC 接收模式。
 */
static void BspUsbPdRxMode(void)
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
 * @brief 对应例程 PD_SINK_Init()：受电模式 + CC 上 5.1k 下拉状态标记。
 */
static void BspUsbPdSinkInit(void)
{
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;
}

/**
 * @brief 对应例程 PD_PHY_Reset()：回到 IDLE，并清掉通信成功标志。
 */
static void BspUsbPdPhyReset(void)
{
    BspUsbPdSinkInit();
    ePdState = STA_IDLE;
    u16PdCommunicationTimerMs = 0u;
    tBspUsbPdStatus.u8ContractValid = 0u;
}

/**
 * @brief 对应例程 PD_Detect()：检测 CC 上是否有 SRC 的 Rp 上拉。
 * @retval 0 无连接；1 = CC1 连接；2 = CC2 连接。
 * @note  例程用 PORT_CC1 & CC_PD 判断当前是否处于 SNK 模式，只有 SNK 才做插入
 *        检测；这里保留该判断。CC 上本板已有 5.1k 外部下拉，故无源时 CC 电平
 *        为低，比较器门限 0.22V 不会误触发。
 */
static uint8_t BspUsbPdDetectCc(void)
{
    uint8_t u8Result = 0u;
    uint8_t u8CmpCc1 = 0u;
    uint8_t u8CmpCc2 = 0u;

    USBPD->PORT_CC1 &= ~(CC_CMP_Mask | PA_CC_AI);
    USBPD->PORT_CC1 |= CC_CMP_22;
    Delay_Us(2u);
    if ((USBPD->PORT_CC1 & PA_CC_AI) != 0u)
    {
        u8CmpCc1 |= bCC_CMP_22;
    }

    USBPD->PORT_CC2 &= ~(CC_CMP_Mask | PA_CC_AI);
    USBPD->PORT_CC2 |= CC_CMP_22;
    Delay_Us(2u);
    if ((USBPD->PORT_CC2 & PA_CC_AI) != 0u)
    {
        u8CmpCc2 |= bCC_CMP_22;
    }

    if ((USBPD->PORT_CC1 & CC_PD) != 0u)
    {
        if ((u8CmpCc1 & bCC_CMP_22) == bCC_CMP_22)
        {
            u8Result = 1u;
        }
        if ((u8CmpCc2 & bCC_CMP_22) == bCC_CMP_22)
        {
            if (u8Result != 0u)
            {
                u8Result = 1u;   /* 华为 A-to-C 线在两条 CC 上都有上拉 */
            }
            else
            {
                u8Result = 2u;
            }
        }
    }

    return u8Result;
}

/**
 * @brief 对应例程 PD_Det_Proc()：检测到稳定连接后锁定 CC 通道并进入 SRC_CONNECT。
 * @note  例程要求连续 5 次命中（每 4ms 一次）才认为插好。
 */
static void BspUsbPdDetectProc(void)
{
    uint8_t u8Status;

    if (tBspUsbPdStatus.u8Connected != 0u)
    {
        /* 已连接：例程此处靠 VBUS 电压判断拔出，本工程暂不处理拔出。 */
        return;
    }

    u8Status = BspUsbPdDetectCc();
    if (u8Status == 0u)
    {
        u8PdDetectCount = 0u;
        return;
    }

    u8PdDetectCount++;
    if (u8PdDetectCount < 5u)
    {
        return;
    }

    u8PdDetectCount = 0u;
    tBspUsbPdStatus.u8Connected = 1u;
    tBspUsbPdStatus.u8CcLine = u8Status;

    if (((USBPD->PORT_CC1 & CC_PD) != 0u) || ((USBPD->PORT_CC2 & CC_PD) != 0u))
    {
        if (u8Status == 1u)
        {
            USBPD->CONFIG &= ~CC_SEL;
        }
        else
        {
            USBPD->CONFIG |= CC_SEL;
        }
        ePdState = STA_SRC_CONNECT;
        printf("[PD] CC%u SRC Connect\r\n", (unsigned int)u8Status);
    }

    u16PdCommunicationTimerMs = 0u;
}

/**
 * @brief 对应例程 PD_Phy_SendPack()：发一个 BMC 包。
 * @param[in] u8Mode 非 0 时发完等待 IF_TX_END，并立刻切回接收模式等 GoodCRC。
 * @param[in] pu8Buffer 报文缓冲；发 Hard Reset 时为 0。
 * @param[in] u8Length 报文长度。
 * @param[in] u8Sop SOP 选择。
 */
static void BspUsbPdSendPhy(uint8_t u8Mode, uint8_t *pu8Buffer,
                            uint8_t u8Length, uint8_t u8Sop)
{
    if ((USBPD->CONFIG & CC_SEL) == CC_SEL)
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

    if (u8Mode != 0u)
    {
        /* 例程注释：发送一定会完成，因此不做超时。 */
        while ((USBPD->STATUS & IF_TX_END) == 0u)
        {
        }
        USBPD->STATUS |= IF_TX_END;

        if ((USBPD->CONFIG & CC_SEL) == CC_SEL)
        {
            USBPD->PORT_CC2 &= ~CC_LVE;
        }
        else
        {
            USBPD->PORT_CC1 &= ~CC_LVE;
        }

        /* 切到接收，准备收 GoodCRC */
        USBPD->CONFIG |= PD_ALL_CLR;
        USBPD->CONFIG &= ~PD_ALL_CLR;
        USBPD->CONTROL &= ~PD_TX_EN;
        USBPD->DMA = (uint32_t)au8PdRxBuffer;
        USBPD->BMC_CLK_CNT = UPD_TMR_RX_48M;
        USBPD->CONTROL |= BMC_START;
    }
}

/**
 * @brief 对应例程 PD_Load_Header()：组装报文头。
 * @note  Header 布局：
 *          bit15        扩展报文
 *          bit[14:12]   数据对象个数（发送时由 BspUsbPdSendMessage 填）
 *          bit[11:9]    消息 ID
 *          bit8         电源角色：0 = SINK
 *          bit[7:6]      协议版本：01 = PD2.0
 *          bit5         数据角色：0 = UFP
 *          bit[4:0]     消息类型
 *        Msg_ID 左对齐存放，因此 & 0x0E 等价于 <<1，保持与例程逐字一致。
 */
static void BspUsbPdLoadHeader(uint8_t u8Extended, uint8_t u8MessageType)
{
    au8PdTxBuffer[0] = u8MessageType;
    /* PD_Ctl.Flag.Bit.PD_Role == 0（受电端），不加 0x20 */
    /* PD_Ctl.Flag.Bit.PD_Version == 0（PD2.0），按例程加 0x40 */
    au8PdTxBuffer[0] |= 0x40u;

    au8PdTxBuffer[1] = (uint8_t)(u8PdMessageId & 0x0Eu);
    /* PD_Ctl.Flag.Bit.PR_Role == 0（SINK），不加 0x01 */
    if (u8Extended != 0u)
    {
        au8PdTxBuffer[1] |= 0x80u;
    }
}

/**
 * @brief 对应例程 PD_Send_Handle()：发送并等待 GoodCRC，最多尝试 3 次。
 * @param[in] pu8Payload 数据对象；控制报文传 0。
 * @param[in] u8Length 负载长度，必须是 4 的倍数且不超过 28。
 * @retval E_OK 收到 GoodCRC。
 * @retval E_ERROR 全部尝试超时。
 * @note  等待窗口 = 250 次 × (3us + 循环开销) ≈ 750us，和例程一致。
 */
static eStatusDef BspUsbPdSendMessage(uint8_t *pu8Payload, uint8_t u8Length)
{
    uint8_t    u8TryCount;
    uint8_t    u8Count;
    uint16_t   u16Timeout;
    eStatusDef eResult = E_ERROR;

    if ((u8Length % 4u) != 0u)
    {
        return E_ERROR;
    }
    if (u8Length > 28u)
    {
        return E_ERROR;
    }

    u8Count = (uint8_t)(u8Length >> 2u);
    au8PdTxBuffer[1] |= (uint8_t)(u8Count << 4u);
    for (u8Count = 0u; u8Count != u8Length; u8Count++)
    {
        au8PdTxBuffer[2u + u8Count] = pu8Payload[u8Count];
    }

    u8TryCount = 4u;
    while (--u8TryCount != 0u)              /* 最多执行 3 次 */
    {
        NVIC_DisableIRQ(USBPD_IRQn);
        BspUsbPdSendPhy(1u, au8PdTxBuffer, (uint8_t)(u8Length + 2u), UPD_SOP0);

        /* 收 GoodCRC 超时 750us */
        u16Timeout = 250u;
        while (--u16Timeout != 0u)
        {
            if ((USBPD->STATUS & IF_RX_ACT) == IF_RX_ACT)
            {
                USBPD->STATUS |= IF_RX_ACT;
                if ((USBPD->BMC_BYTE_CNT == 6u) &&
                    ((au8PdRxBuffer[0] & 0x1Fu) == DEF_TYPE_GOODCRC))
                {
                    u8PdMessageId = (uint8_t)(u8PdMessageId + 2u);
                    u16PdTxCount++;
                    eResult = E_OK;
                    break;
                }
            }
            Delay_Us(3u);
        }

        if (eResult == E_OK)
        {
            break;
        }
    }

    BspUsbPdRxMode();

    return eResult;
}

/**
 * @brief 对应例程 PDO_Request()：按固定档位号发出 Request。
 * @param[in] u8PdoIndex 档位号，从 1 开始。
 * @note  Request Data Object 直接复用接收缓冲的前 4 字节（与例程一致）。
 */
static void BspUsbPdStartPdoRequest(uint8_t u8PdoIndex)
{
    uint16_t   CurrentMa;
    uint16_t   VoltageMv;
    eStatusDef eStatus;

    if ((u8PdoIndex > tBspUsbPdStatus.u8PdoCount) || (u8PdoIndex == 0u))
    {
        printf("[PD] pdo_index error!\r\n");
        return;
    }

    memcpy(&au8PdRxBuffer[2], &au8PdSourceCapabilities[4u * (u8PdoIndex - 1u) + 1u], 4u);
    BspUsbPdDecodeFixedPdo(1u, &au8PdRxBuffer[2], &CurrentMa, &VoltageMv);

    tBspUsbPdStatus.u8RequestedPdo = u8PdoIndex;
    tBspUsbPdStatus.u16CurrentMa = CurrentMa;
    tBspUsbPdStatus.u16VoltageMv = VoltageMv;
    printf("[PD] request PDO%u: %u mV %u mA\r\n",
           (unsigned int)u8PdoIndex,
           (unsigned int)VoltageMv,
           (unsigned int)CurrentMa);

    BspUsbPdLoadHeader(0x00u, DEF_TYPE_REQUEST);
    au8PdRxBuffer[5] = 0x03u;
    au8PdRxBuffer[5] |= (uint8_t)(u8PdoIndex << 4);
    au8PdRxBuffer[3] = au8PdRxBuffer[3] & 0x03u;
    au8PdRxBuffer[3] |= (uint8_t)(au8PdRxBuffer[2] << 2);
    au8PdRxBuffer[4] = au8PdRxBuffer[3];
    au8PdRxBuffer[4] <<= 2;
    au8PdRxBuffer[4] = au8PdRxBuffer[4] & 0x0Cu;
    au8PdRxBuffer[4] |= (uint8_t)(au8PdRxBuffer[2] >> 6);

    eStatus = BspUsbPdSendMessage(&au8PdRxBuffer[2], 4u);

    if (eStatus == E_OK)
    {
        ePdState = STA_RX_ACCEPT_WAIT;
    }
    else
    {
        ePdState = STA_TX_SOFTRST;
    }
    u16PdCommunicationTimerMs = 0u;
}

/**
 * @brief 对应例程 PD_Save_Adapter_SrcCap()：保存电源能力表（去掉 PPS 段）。
 */
static void BspUsbPdSaveSourceCapabilities(void)
{
    uint8_t u8Index;
    uint8_t u8Length;

    u8Length = (uint8_t)((au8PdRxBuffer[1] >> 4u) & 0x07u);

    for (u8Index = 0u; u8Index < u8Length; u8Index++)
    {
        if ((au8PdRxBuffer[2u + (u8Index << 2) + 3u] & 0xC0u) == 0xC0u)
        {
            break;
        }
    }

    tBspUsbPdStatus.u8PdoCount = u8Index;

    au8PdRxBuffer[5] = 0x3Eu;
    au8PdRxBuffer[1] &= 0x8Fu;
    au8PdRxBuffer[1] |= (uint8_t)(u8Index << 4);
    au8PdSourceCapabilities[0] = u8Index;
    memcpy(&au8PdSourceCapabilities[1], &au8PdRxBuffer[2], (size_t)(u8Index << 2));
}

/**
 * @brief 对应例程 PD_PDO_Analyse()：解析固定档位的电压/电流。
 */
static void BspUsbPdDecodeFixedPdo(uint8_t u8PdoIndex, const uint8_t *pu8SourceCap,
                                   uint16_t *pu16CurrentMa, uint16_t *pu16VoltageMv)
{
    uint32_t u32Temp;

    u32Temp = pu8SourceCap[((u8PdoIndex - 1u) << 2) + 0u] +
              ((uint32_t)pu8SourceCap[((u8PdoIndex - 1u) << 2) + 1u] << 8) +
              ((uint32_t)pu8SourceCap[((u8PdoIndex - 1u) << 2) + 2u] << 16);

    if (pu16CurrentMa != 0)
    {
        *pu16CurrentMa = (uint16_t)((u32Temp & 0x000003FFu) * 10u);
    }

    if (pu16VoltageMv != 0)
    {
        u32Temp = u32Temp >> 10;
        *pu16VoltageMv = (uint16_t)((u32Temp & 0x000003FFu) * 50u);
    }
}

/**
 * @brief 对应例程 PD_Init()：PD 外设与状态初始化。
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

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBPD, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;
    USBPD->CONFIG = PD_DMA_EN;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE |
                    IF_RX_ACT | IF_RX_RESET | IF_TX_END;

    NVIC_InitStructure.NVIC_IRQChannel = USBPD_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0u;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1u;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    u8PdMessageId = 0u;
    u8PdDetectTimerMs = 0u;
    u8PdDetectCount = 0u;
    u8PdErrOpCount = 0u;
    u8PdMessageReceived = 0u;
    u16PdCommunicationTimerMs = 0u;
    u8PdMsDelta = 0u;
    u16PdTxCount = 0u;
    u16PdRxCount = 0u;

    au8PdSourceCapabilities[0] = 1u;

    BspUsbPdPhyReset();
    BspUsbPdRxMode();

    printf("[PD] driver %s\r\n", BSP_USB_PD_VERSION_STR);
    printf("[PD] sink initialized: CC1=PC14 CC2=PC15 request=PDO%u\r\n",
           (unsigned int)USER_PD_REQUEST_PDO_INDEX);
}

/**
 * @brief 对应例程 main() 循环体 + PD_Main_Proc()。
 * @param[in] u16ElapsedMs 距上次调用的毫秒数（由用户任务传入）。
 */
void BspUsbPdProcess(uint16_t u16ElapsedMs)
{
    uint8_t    u8PdHeader;
    uint8_t    u8Index;
    uint16_t   CurrentMa;
    uint16_t   VoltageMv;
    eStatusDef eStatus;

    u8PdMsDelta = (uint8_t)((u16ElapsedMs > 255u) ? 255u : u16ElapsedMs);

    /* ---- 例程 main()：Det_Timer > 4 才做一次连接检测 ---- */
    u8PdDetectTimerMs = (uint8_t)(u8PdDetectTimerMs + u8PdMsDelta);
    if (u8PdDetectTimerMs > 4u)
    {
        u8PdDetectTimerMs = 0u;
        BspUsbPdDetectProc();
    }

    /* ---- 例程 PD_Main_Proc()：状态处理 ---- */
    switch (ePdState)
    {
    case STA_DISCONNECT:
        printf("[PD] Disconnect\r\n");
        BspUsbPdPhyReset();
        break;

    case STA_SRC_CONNECT:
        /* 1s 内没收到 SrcCap 就重试，超过 5 次放弃 */
        u16PdCommunicationTimerMs = (uint16_t)(u16PdCommunicationTimerMs + u8PdMsDelta);
        if (u16PdCommunicationTimerMs > 999u)
        {
            u8PdErrOpCount++;
            printf("[PD] SrcCap timeout #%u: cfg=%08lx stat=%08lx cnt=%lu\r\n",
                   (unsigned int)u8PdErrOpCount,
                   (unsigned long)USBPD->CONFIG,
                   (unsigned long)USBPD->STATUS,
                   (unsigned long)USBPD->BMC_BYTE_CNT);
            if (u8PdErrOpCount > 5u)
            {
                u8PdErrOpCount = 0u;
                ePdState = STA_IDLE;
            }
            else
            {
                BspUsbPdPhyReset();
                ePdState = STA_SRC_CONNECT;
                BspUsbPdRxMode();
            }
            u16PdCommunicationTimerMs = 0u;
        }
        break;

    case STA_RX_ACCEPT_WAIT:
    case STA_RX_PS_RDY_WAIT:
        u16PdCommunicationTimerMs = (uint16_t)(u16PdCommunicationTimerMs + u8PdMsDelta);
        if (u16PdCommunicationTimerMs > 499u)
        {
            ePdState = STA_TX_SOFTRST;
            u16PdCommunicationTimerMs = 0u;
        }
        break;

    case STA_RX_PS_RDY:
        ePdState = STA_IDLE;
        break;

    case STA_TX_SOFTRST:
        BspUsbPdLoadHeader(0x00u, DEF_TYPE_SOFT_RESET);
        eStatus = BspUsbPdSendMessage(0, 0u);
        if (eStatus == E_OK)
        {
            ePdState = STA_IDLE;
        }
        else
        {
            ePdState = STA_TX_HRST;
        }
        u16PdCommunicationTimerMs = 0u;
        break;

    case STA_TX_HRST:
        BspUsbPdSendPhy(0x01u, 0, 0u, UPD_HARD_RESET);
        BspUsbPdRxMode();
        ePdState = STA_IDLE;
        u16PdCommunicationTimerMs = 0u;
        break;

    default:
        break;
    }

    /* ---- 例程 PD_Main_Proc()：收到报文后的处理 ---- */
    if (u8PdMessageReceived != 0u)
    {
        u8PdHeader = (uint8_t)(au8PdRxBuffer[0] & 0x1Fu);
        u16PdRxCount++;
        switch (u8PdHeader)
        {
        case DEF_TYPE_SRC_CAP:
            Delay_Ms(5u);
            BspUsbPdSaveSourceCapabilities();

            printf("[PD] SrcCap: %u fixed PDO(s) (tx=%u rx=%u)\r\n",
                   (unsigned int)tBspUsbPdStatus.u8PdoCount,
                   (unsigned int)u16PdTxCount, (unsigned int)u16PdRxCount);
            for (u8Index = 1u; u8Index <= tBspUsbPdStatus.u8PdoCount; ++u8Index)
            {
                BspUsbPdDecodeFixedPdo(u8Index, &au8PdSourceCapabilities[1],
                                       &CurrentMa, &VoltageMv);
                printf("[PD] PDO%u: %u mV %u mA\r\n",
                       (unsigned int)u8Index,
                       (unsigned int)VoltageMv,
                       (unsigned int)CurrentMa);
            }
            /* 例程默认申请第 5 组 PDO（20V）；本工程按配置项申请。 */
            u8Index = USER_PD_REQUEST_PDO_INDEX;
            if ((u8Index == 0u) || (u8Index > tBspUsbPdStatus.u8PdoCount))
            {
                u8Index = 1u;
            }
            BspUsbPdStartPdoRequest(u8Index);
            break;

        case DEF_TYPE_ACCEPT:
            ePdState = STA_RX_PS_RDY_WAIT;
            u16PdCommunicationTimerMs = 0u;
            printf("[PD] ACCEPT (tx=%u rx=%u)\r\n",
                   (unsigned int)u16PdTxCount, (unsigned int)u16PdRxCount);
            break;

        case DEF_TYPE_PS_RDY:
            printf("[PD] PS_RDY: contract %u mV %u mA\r\n",
                   (unsigned int)tBspUsbPdStatus.u16VoltageMv,
                   (unsigned int)tBspUsbPdStatus.u16CurrentMa);
            ePdState = STA_IDLE;
            tBspUsbPdStatus.u8ContractValid = 1u;
            break;

        case DEF_TYPE_WAIT:
            break;

        case DEF_TYPE_GET_SNK_CAP:
            Delay_Ms(1u);
            BspUsbPdLoadHeader(0x00u, DEF_TYPE_SNK_CAP);
            (void)BspUsbPdSendMessage((uint8_t *)au8PdSinkCapability,
                                      sizeof(au8PdSinkCapability));
            break;

        case DEF_TYPE_SOFT_RESET:
            Delay_Ms(1u);
            BspUsbPdLoadHeader(0x00u, DEF_TYPE_ACCEPT);
            (void)BspUsbPdSendMessage(0, 0u);
            break;

        case DEF_TYPE_GET_SRC_CAP_EX:
            Delay_Ms(1u);
            /* 本工程不声明扩展能力，用 REJECT 明确回绝 */
            BspUsbPdLoadHeader(0x00u, DEF_TYPE_REJECT);
            (void)BspUsbPdSendMessage(0, 0u);
            break;

        case DEF_TYPE_VCONN_SWAP:
        case DEF_TYPE_PR_SWAP:
        case DEF_TYPE_DR_SWAP:
            Delay_Ms(1u);
            BspUsbPdLoadHeader(0x00u, DEF_TYPE_REJECT);
            (void)BspUsbPdSendMessage(0, 0u);
            break;

        default:
            printf("[PD] msg 0x%02x ignored (hdr=%02x %02x)\r\n",
                   (unsigned int)u8PdHeader,
                   (unsigned int)au8PdRxBuffer[0],
                   (unsigned int)au8PdRxBuffer[1]);
            break;
        }

        /* 报文处理完，重新开收 */
        BspUsbPdRxMode();
        u8PdMessageReceived = 0u;
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

    if ((ePdState != STA_IDLE) || (u8PdMessageReceived != 0u))
    {
        return E_BUSY;
    }

    tBspUsbPdStatus.u8ContractValid = 0u;
    BspUsbPdStartPdoRequest(u8PdoIndex);

    return E_OK;
}

/**
 * @brief 对应例程 USBPD_IRQHandler()。
 */
void USBPD_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBPD_IRQHandler(void)
{
    if ((USBPD->STATUS & IF_RX_ACT) != 0u)
    {
        USBPD->STATUS |= IF_RX_ACT;
        if ((USBPD->STATUS & MASK_PD_STAT) == PD_RX_SOP0)
        {
            if (USBPD->BMC_BYTE_CNT >= 6u)
            {
                /* 是 GoodCRC 就不应答、直接忽略 */
                if ((USBPD->BMC_BYTE_CNT != 6u) ||
                    ((au8PdRxBuffer[0] & 0x1Fu) != DEF_TYPE_GOODCRC))
                {
                    Delay_Us(30u);                 /* 延时 30us 后回 GoodCRC */
                    au8PdAckBuffer[0] = 0x41u;
                    /* 例程此处 |= PD_Ctl.Flag.Bit.Auto_Ack_PRRole，SNK 下该位为 0 */
                    au8PdAckBuffer[1] = (uint8_t)(au8PdRxBuffer[1] & 0x0Eu);
                    USBPD->CONFIG |= IE_TX_END;
                    BspUsbPdSendPhy(0u, au8PdAckBuffer, 2u, UPD_SOP0);
                }
            }
        }
    }

    if ((USBPD->STATUS & IF_TX_END) != 0u)
    {
        /* 包发送完成（只会是 GoodCRC 发完） */
        USBPD->PORT_CC1 &= ~CC_LVE;
        USBPD->PORT_CC2 &= ~CC_LVE;

        /* 关中断，等主循环处理完再打开 */
        NVIC_DisableIRQ(USBPD_IRQn);
        u8PdMessageReceived = 1u;
        USBPD->STATUS |= IF_TX_END;
    }

    if ((USBPD->STATUS & IF_RX_RESET) != 0u)
    {
        USBPD->STATUS |= IF_RX_RESET;
        BspUsbPdSinkInit();
        printf("[PD] IF_RX_RESET\r\n");
    }
}
