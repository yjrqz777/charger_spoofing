/**
 * @file    user_system.c
 * @brief   系统任务与状态管理实�?
 *******************************************************************************
 * @note    提供系统初始化、状态机管理以及电流数据格式化输出的功能�?
 *          通过 Protothread 协程周期性采集并输出电流数据�?RTT�?
 *          系统状态包�?INIT -> POWER_ON -> RUNNING -> OFF 四个阶段�?
 *******************************************************************************
 */

#include "user_system.h"
#include "bsp_adc.h"
#include "bsp_hall.h"
#include "bsp_mt6816ct_acd.h"
#include "user_foc.h"
#include "user_motor.h"
tSysDataDef tSysData;

#define USER_SYSTEM_MT6816_POLL_DIVIDER      (20u)     /* 5 ms task * 20 = 100 ms per RTT line. */

/** @brief 三位小数电流格式化结构体（用�?RTT CSV 输出�?*/
typedef struct tCurrentDecimal3Def
{
    const char * pcSign;      /**< 符号串，"" �?"-" */
    uint32_t u32Integer;      /**< 整数部分（A�?*/
    uint32_t u32Fraction;     /**< 小数部分（毫安，千分位） */
} tCurrentDecimal3Def;

/**
 * @brief  将电流值（A）转换为毫安整数（四舍五入）
 * @param[in] f32Current  电流值，单位 A
 * @return 电流的毫安等效值（正数向上取整，负数向下取整）
 * @note   用于 RTT 输出的格式化转换
 */
static int32_t UsrSystemCurrentToMilliAmp(float f32Current)
{
    if (f32Current >= 0.0f)
    {
        return (int32_t)((f32Current * 1000.0f) + 0.5f);
    }

    return (int32_t)((f32Current * 1000.0f) - 0.5f);
}

/**
 * @brief  将电流值（A）转换为带符号和小数部分的十进制结构�?
 * @param[in] f32Current  电流值，单位 A
 * @return tCurrentDecimal3Def 结构体，包含符号串、整数部分和小数部分（千分位�?
 * @note   用于 RTT CSV 格式输出，格式如 "-1.234" �?"0.567"
 */
static tCurrentDecimal3Def UsrSystemCurrentToDecimal3(float f32Current)
{
    int32_t MilliAmp = UsrSystemCurrentToMilliAmp(f32Current);
    uint32_t AbsoluteMilliAmp;
    tCurrentDecimal3Def Decimal;

    if (MilliAmp < 0)
    {
        AbsoluteMilliAmp = (uint32_t)(-MilliAmp);
        Decimal.pcSign = "-";
    }
    else
    {
        AbsoluteMilliAmp = (uint32_t)MilliAmp;
        Decimal.pcSign = "";
    }

    Decimal.u32Integer = AbsoluteMilliAmp / 1000u;
    Decimal.u32Fraction = AbsoluteMilliAmp % 1000u;

    return Decimal;
}


/**
 * @brief  Read and print one MT6816CT-ACD sample without using floating point.
 * @param[in] pcTag RTT record tag, for example "init" or "poll".
 * @note   The angle is printed in centi-degrees so RTT printf does not depend
 *         on the optional floating-point printf implementation.
 */
static void UsrSystemPrintMt6816(const char *pcTag)
{
    tBspMt6816CtSampleDef Sample;
    uint32_t u32AngleCdeg;
    int32_t s32SpeedRefDeci;
    int32_t s32SpeedMeasDeci;
    int32_t s32IqMilliAmp;

    if (BspMt6816CtIsSampleValid() == 0u)
    {
        SEGGER_RTT_printf(0, "MT6816,%s,sample=unavailable\r\n", pcTag);
        return;
    }
    BspMt6816CtGetLastSample(&Sample);
}
/**
 * @brief  初始化系统状态和数据
 * @note   设置系统状态为 E_SYS_STATE_INIT，清零运行计数，
 *         并通过 RTT 输出电流 CSV 格式标题行，�?J-Scope 数据采集
 */
void UsrSystemInit(void)
{
    tSysData.eState = E_SYS_STATE_INIT;
    tSysData.u32PowerOnTimes = 0u;
    tSysData.u32OpenTimes = 0u;
    SEGGER_RTT_WriteString(0, "FOC,seq,mode,adc,id,iq,spd_ref,spd_tgt,sum,hall,hvalid,angle,speed,off_cal,off,oc\r\n");
}


/**
 * @brief  Protothread 系统协程任务 �?周期性采集并输出电流数据
 * @return PT 状态码
 * @note   首次进入时初始化系统，之后按 USR_SYSTEM_TASK_INTERVAL_MS 周期执行�?
 *         - 轮询更新 ADC2 通道值（母线电压、电位器等）
 *         - 采集三相电流 (Ia/Ib/Ic) �?dq 轴电�?(Id/Iq)
 *         - 通过 RTT 输出 CSV 格式数据流，供上位机�?J-Scope 采集
 */
/**
 * @brief  速度目标门控：按系统状态更新速度目标
 * @note   RUNNING 状态下读取电位器值传入，非 RUNNING 状态下速度目标置零。
 *         由 UsrSystemTask 以 5ms 周期调用，无需 1kHz 运行。
 */
static void UsrSystemUpdateSpeedGate(void)
{
    if (tSysData.eState == E_SYS_STATE_RUNNING)
    {
        UsrMotorSetSpeedTarget(UsrMotorGetSpeedRef());   /* RUNNING: 传入电位器速度 */
    }
    else
    {
        UsrMotorSetSpeedTarget(0.0f);                          /* 非RUNNING: 速度目标置零 */
    }
}

uint16_t UsrSystemTask(void)
{
    static uint8_t u8Mt6816PollDivider = 0u;

    PT_BEGIN()
    {
        UsrSystemInit();
        UsrSystemPrintMt6816("init");
    }

    while (1)
    {
        PT_WAIT_UNTIL(USR_SYSTEM_TASK_INTERVAL_MS / OS_TICK_MS);

        // u8Mt6816PollDivider++;
        // if (u8Mt6816PollDivider >= USER_SYSTEM_MT6816_POLL_DIVIDER)
        // {
        //     u8Mt6816PollDivider = 0u;
        //     UsrSystemPrintMt6816("poll");
        // }
        // tCurrentDecimal3Def CurrentSum;
        // tCurrentDecimal3Def DirectCurrent;
        // tCurrentDecimal3Def QuadratureCurrent;
        // tCurrentDecimal3Def SpeedReference;
        // tCurrentDecimal3Def SpeedTarget;
        // tDqCurrentDef DqCurrent;
        // tBspAdcCurrentSnapshotDef CurrentSnapshot;

        // PT_WAIT_UNTIL(USR_SYSTEM_TASK_INTERVAL_MS / OS_TICK_MS);

        (void)BspAdc2UpdateAll();

        UsrSystemUpdateSpeedGate();   /* 按系统状态门控速度目标(5ms) */

        // if ((u8OffsetPrinted == 0u) && (BspAdcIsCurrentOffsetReady() != 0u))
        // {
        //     tBspAdcCalDebugDef CalibrationDebug;

        //     BspAdcGetCalDebug(&CalibrationDebug);

        //     SEGGER_RTT_printf(0, "CAL_DONE,retry=%u,state=%u\r\n",
        //                       (unsigned)CalibrationDebug.u8RetryCount, (unsigned)CalibrationDebug.eState);
        //     SEGGER_RTT_printf(0, "CAL_A,off=%u,min=%u,max=%u,span=%u,drift=%d\r\n",
        //                       (unsigned)CalibrationDebug.u16Offset[0], (unsigned)CalibrationDebug.u16MinRaw[0],
        //                       (unsigned)CalibrationDebug.u16MaxRaw[0], (unsigned)CalibrationDebug.u16Span[0],
        //                       (int)CalibrationDebug.s16Drift[0]);
        //     SEGGER_RTT_printf(0, "CAL_B,off=%u,min=%u,max=%u,span=%u,drift=%d\r\n",
        //                       (unsigned)CalibrationDebug.u16Offset[1], (unsigned)CalibrationDebug.u16MinRaw[1],
        //                       (unsigned)CalibrationDebug.u16MaxRaw[1], (unsigned)CalibrationDebug.u16Span[1],
        //                       (int)CalibrationDebug.s16Drift[1]);
        //     SEGGER_RTT_printf(0, "CAL_C,off=%u,min=%u,max=%u,span=%u,drift=%d\r\n",
        //                       (unsigned)CalibrationDebug.u16Offset[2], (unsigned)CalibrationDebug.u16MinRaw[2],
        //                       (unsigned)CalibrationDebug.u16MaxRaw[2], (unsigned)CalibrationDebug.u16Span[2],
        //                       (int)CalibrationDebug.s16Drift[2]);
        //     u8OffsetPrinted = 1u;
        // }

        // u8RttPrintDivider++;
        // if (u8RttPrintDivider >= USER_SYSTEM_RTT_PRINT_DIVIDER)
        // {
        //     u8RttPrintDivider = 0u;

        //     BspAdcGetCurrentSnapshot(&CurrentSnapshot);
        //     CurrentSum = UsrSystemCurrentToDecimal3(CurrentSnapshot.f32CurrentSum);
        //     DqCurrent = UsrFocGetDqCurrent();
        //     DirectCurrent = UsrSystemCurrentToDecimal3(DqCurrent.f32D);
        //     QuadratureCurrent = UsrSystemCurrentToDecimal3(DqCurrent.f32Q);
        //     SpeedReference = UsrSystemCurrentToDecimal3(UsrMotorGetSpeedRef());
        //     SpeedTarget = UsrSystemCurrentToDecimal3(UsrMotorGetSpeedRef());

        //     SEGGER_RTT_printf(0, "FOC,seq=%u,mode=%u,adc=%u,id=%s%u.%03u,iq=%s%u.%03u,spd_ref=%s%u.%03u,spd_tgt=%s%u.%03u,sum=%s%u.%03u,hall=%u,hvalid=%u,angle=%d,speed=%d,off_cal=%u,off=%d,oc=%u\r\n",
        //                       (unsigned)CurrentSnapshot.u32Sequence,
        //                       (unsigned)UsrMotorGetStartupMode(),
        //                       (unsigned)CurrentSnapshot.u8SampleValid,
        //                       DirectCurrent.pcSign, (unsigned)DirectCurrent.u32Integer, (unsigned)DirectCurrent.u32Fraction,
        //                       QuadratureCurrent.pcSign, (unsigned)QuadratureCurrent.u32Integer, (unsigned)QuadratureCurrent.u32Fraction,
        //                       SpeedReference.pcSign, (unsigned)SpeedReference.u32Integer, (unsigned)SpeedReference.u32Fraction,
        //                       SpeedTarget.pcSign, (unsigned)SpeedTarget.u32Integer, (unsigned)SpeedTarget.u32Fraction,
        //                       CurrentSum.pcSign, (unsigned)CurrentSum.u32Integer, (unsigned)CurrentSum.u32Fraction,
        //                       (unsigned)BspHallGetState(),
        //                       (unsigned)BspHallIsAngleValid(),
        //                       (int)(BspHallGetElectricalAngle() * 57.2958f),
        //                       (int)(BspHallGetElectricalSpeed() * 1000.0f),
        //                       (unsigned)BspHallIsOffsetCalibrated(),
        //                       (int)(BspHallGetOffset() * 57.2958f),
        //                       (unsigned)UsrMotorIsOverCurrentFault());
        // }
    }

    PT_END();
}
