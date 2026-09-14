/**
 * @file user_ws2812.c
 * @brief Implements the non-blocking WS2812 color-fade demonstration.
 */

#include "user_ws2812.h"
#include "bsp_ws2812.h"

static const uint32_t au32Ws2812Colors[] =
{
    0x100000u,
    0x001000u,
    0x000010u,
    0x101000u,
    0x100010u,
    0x001010u,
    0x101010u
};

static uint8_t u8Ws2812ColorIndex = 0u;
static uint16_t u16Ws2812FadeStep = 0u;

/**
 * @brief Interpolates two packed RGB888 colors using integer arithmetic.
 * @param[in] u32StartColor Start RGB888 color.
 * @param[in] u32EndColor End RGB888 color.
 * @param[in] u16Step Interpolation step.
 * @return Interpolated RGB888 color.
 */
static uint32_t UsrWs2812Interpolate(uint32_t u32StartColor,
                                     uint32_t u32EndColor,
                                     uint16_t u16Step)
{
    int32_t StartRed;
    int32_t StartGreen;
    int32_t StartBlue;
    int32_t EndRed;
    int32_t EndGreen;
    int32_t EndBlue;
    uint32_t Red;
    uint32_t Green;
    uint32_t Blue;

    StartRed = (int32_t)((u32StartColor >> 16u) & 0xFFu);
    StartGreen = (int32_t)((u32StartColor >> 8u) & 0xFFu);
    StartBlue = (int32_t)(u32StartColor & 0xFFu);
    EndRed = (int32_t)((u32EndColor >> 16u) & 0xFFu);
    EndGreen = (int32_t)((u32EndColor >> 8u) & 0xFFu);
    EndBlue = (int32_t)(u32EndColor & 0xFFu);

    Red = (uint32_t)(StartRed + ((EndRed - StartRed) * (int32_t)u16Step) /
                                (int32_t)USR_WS2812_FADE_STEPS);
    Green = (uint32_t)(StartGreen + ((EndGreen - StartGreen) * (int32_t)u16Step) /
                                    (int32_t)USR_WS2812_FADE_STEPS);
    Blue = (uint32_t)(StartBlue + ((EndBlue - StartBlue) * (int32_t)u16Step) /
                                   (int32_t)USR_WS2812_FADE_STEPS);

    return (Red << 16u) | (Green << 8u) | Blue;
}

/**
 * @brief Resets the color-fade animation state.
 */
void UsrWs2812Init(void)
{
    u8Ws2812ColorIndex = 0u;
    u16Ws2812FadeStep = 0u;
}

/**
 * @brief Advances the shared color of all four WS2812 pixels.
 */
void UsrWs2812Update(void)
{
    uint8_t NextColorIndex;
    uint32_t Color;

    if (BspWs2812IsIdle() == 0u)
    {
        return;
    }

    NextColorIndex = (uint8_t)(u8Ws2812ColorIndex + 1u);
    if (NextColorIndex >= (uint8_t)(sizeof(au32Ws2812Colors) /
                                    sizeof(au32Ws2812Colors[0])))
    {
        NextColorIndex = 0u;
    }

    Color = UsrWs2812Interpolate(au32Ws2812Colors[u8Ws2812ColorIndex],
                                 au32Ws2812Colors[NextColorIndex],
                                 u16Ws2812FadeStep);
    (void)BspWs2812Fill((uint8_t)(Color >> 16u),
                        (uint8_t)(Color >> 8u),
                        (uint8_t)Color);
    (void)BspWs2812Show();

    u16Ws2812FadeStep++;
    if (u16Ws2812FadeStep >= USR_WS2812_FADE_STEPS)
    {
        u16Ws2812FadeStep = 0u;
        u8Ws2812ColorIndex = NextColorIndex;
    }
}

/**
 * @brief Runs the non-blocking WS2812 animation task.
 * @return Protothread wait interval in scheduler ticks.
 */
uint16_t UsrWs2812Task(void)
{
    PT_BEGIN()
    {
        UsrWs2812Init();
    }

    while (1)
    {
        PT_WAIT_UNTIL(USR_WS2812_TASK_INTERVAL_MS / OS_TICK_MS);
        UsrWs2812Update();
    }

    PT_END();
}
