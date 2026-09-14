/**
 * @file bsp_button.h
 * @brief Declares the board button scanner and multi-button interfaces.
 */

/***************************************************************************************************
 * Author: yjrqz777 3210551161@qq.com
 * Date: 2026-01-06 19:59:07
 * LastEditTime: 2026-08-06 17:58:10
 * LastEditors: duanzhixuan duanzhixuan@topband.com.cn
 * Description: 
 * FilePath: \MyFoc\Code\UserBsp\bsp_button.h
 * @YJRQZ777
***************************************************************************************************/

#ifndef __BSP_BUTTON_H__
#define __BSP_BUTTON_H__

#include <stdint.h>
#include <string.h>

// Configuration constants - can be modified according to your needs
#define TICKS_INTERVAL       (5u)                         /* Scanner interval in milliseconds. */
#define DEBOUNCE_TICKS       (2u)                         /* Debounce samples; maximum is 7. */
#define SHORT_TICKS          (100u / TICKS_INTERVAL)      /* Multi-click interval. */
#define LONG_TICKS           (1000u / TICKS_INTERVAL)     /* Long-press interval. */
#define PRESS_REPEAT_MAX_NUM (2u)                         /* Maximum repeat count. */

// Forward declaration
typedef struct _Button Button;

// Button callback function type
typedef void (*BtnCallback)(Button* btn_handle);

// Button event types
typedef enum {
	BTN_PRESS_DOWN = 0,     /* button pressed down       按键按下 */
	BTN_PRESS_UP,           /* button released           按键释放 */
	BTN_PRESS_REPEAT,       /* repeated press detected   检测到重复按下 */
	BTN_SINGLE_CLICK,       /* single click completed    单击完成 */
	BTN_DOUBLE_CLICK,       /* double click completed    双击完成 */
	BTN_LONG_PRESS_START,   /* long press started        长按开始 */
	BTN_LONG_PRESS_HOLD,    /* long press holding        长按持续中 */
	BTN_EVENT_COUNT,        /* total number of events    事件总数 */
	BTN_NONE_PRESS          /* no event                  无事件 */
} ButtonEvent;

// Button state machine states
typedef enum {
	BTN_STATE_IDLE = 0,     /* idle state                空闲状态 */
	BTN_STATE_PRESS,        /* pressed state             按下状态 */
	BTN_STATE_RELEASE,      /* released state waiting    释放等待超时 */
	BTN_STATE_REPEAT,       /* repeat press state        重复按下状态 */
	BTN_STATE_LONG_HOLD     /* long press hold state     长按保持状态 */
} ButtonState;

// Button structure
struct _Button {
	uint16_t ticks;                     // tick counter
	uint8_t  repeat : 4;                // repeat counter (0-15)
	uint8_t  event : 4;                 // current event (0-15)
	uint8_t  state : 3;                 // state machine state (0-7)
	uint8_t  debounce_cnt : 3;          // debounce counter (0-7)
	uint8_t  active_level : 1;          // active GPIO level (0 or 1)
	uint8_t  button_level : 1;          // current button level
	uint8_t  button_id;                 // button identifier
	uint8_t  (*hal_button_level)(uint8_t button_id);  // HAL function to read GPIO
	BtnCallback cb[BTN_EVENT_COUNT];    // callback function array
	volatile uint16_t u16PendingEvents; // events queued by the timer interrupt
	uint8_t u8DispatchedEvent;          // event currently dispatched in main context
	uint8_t u8DispatchActive;           // callback dispatch is active
	Button* next;                       // next button in linked list
};

#ifdef __cplusplus
extern "C" {
#endif /* __BSP_BUTTON_H__ */

// First-party board wrapper API
uint8_t BspButtonReadLevel(uint8_t u8ButtonId);
uint8_t BspButtonGetRawMask(void);
void BspButtonScanTick(void);
void BspButtonProcessEvents(void);

// Generic multi-button public API
void button_init(Button* handle, uint8_t(*pin_level)(uint8_t), uint8_t active_level, uint8_t button_id);
void button_attach(Button* handle, ButtonEvent event, BtnCallback cb);
void button_detach(Button* handle, ButtonEvent event);
ButtonEvent button_get_event(Button* handle);
int  button_start(Button* handle);
void button_stop(Button* handle);
void button_ticks(void);

// Utility functions
uint8_t button_get_repeat_count(Button* handle);
void button_reset(Button* handle);
int button_is_pressed(Button* handle);

#ifdef __cplusplus
}
#endif

#endif
