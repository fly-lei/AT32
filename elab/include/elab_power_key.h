#ifndef __ELAB_POWER_KEY_H
#define __ELAB_POWER_KEY_H

#include "at32f403a_407.h"

/* 硬件引脚物理映射定义 */
#define ELAB_POWER_PORT GPIOD
#define ELAB_POWER_PIN GPIO_PINS_12
#define ELAB_POWER_CRM_CLK CRM_GPIOD_PERIPH_CLOCK

#define ELAB_KEY_PORT GPIOD
#define ELAB_KEY_PIN GPIO_PINS_11
#define ELAB_KEY_CRM_CLK CRM_GPIOD_PERIPH_CLOCK

/* 按键触发事件枚举（可直接映射到上层状态机的 Signal） */
typedef enum
{
    ELAB_KEY_EVT_NONE = 0,
    ELAB_KEY_EVT_SHORT_PRESS, /* 短按事件：切换模式/功能 */
    ELAB_KEY_EVT_LONG_PRESS   /* 长按事件：请求系统关机 */
} elab_key_event_t;

/* 事件回调函数指针原型（用于向外注册） */
typedef void (*elab_key_callback_t)(elab_key_event_t evt);

/* 对外公共 API */
void elab_power_key_init(elab_key_callback_t callback);
void elab_power_key_tick_isr(void);
void elab_system_shutdown(void);

#endif /* __ELAB_POWER_KEY_H */