#ifndef ELAB_LED_STM32_H
#define ELAB_LED_STM32_H

#include "elab_device.h"
#include "wk_gpio.h"
#include "at32f403a_407.h" // 包含 HAL 库以控制 GPIO

/* 1. 定义 LED 实例结构体 (面向对象思想的 C 语言实现) */
typedef struct {
    elab_device_t dev;      /* ⚠️ 必须是第一个成员！这就叫“继承”，方便后续指针强转 */
    gpio_type *port;     
    uint16_t pin;           
} elab_led_inst_t;

/* 2. 声明底层统一的注册入口函数 (实现在 .c 中) */
extern void elab_led_core_register(elab_led_inst_t *inst, elab_device_attr_t *attr);

/* 3. 终极奥义：自动注册宏
 * 作用：自动为你生成静态变量，并打上 eLab 的初始化段标签
 */
#define REGISTER_LED(_name_str, _port, _pin, _level) \
    /* 静态分配设备实例，保存对应的硬件引脚 */ \
    static elab_led_inst_t _inst_##_name_str = { \
        .port = _port, \
        .pin = _pin \
    }; \
    /* 静态分配属性，防止局部变量被销毁 */ \
    static elab_device_attr_t _attr_##_name_str = { \
        .name = #_name_str, \
        .sole = false \
    }; \
    /* 生成专属于这个 LED 的初始化函数 */ \
    static void _init_##_name_str(void) { \
        elab_led_core_register(&_inst_##_name_str, &_attr_##_name_str); \
    } \
    /* 调用你的 eLab 链接器宏将其放入初始化段 */ \
    ELAB_INIT_EXPORT(_init_##_name_str, _level)

#endif /* ELAB_LED_STM32_H */