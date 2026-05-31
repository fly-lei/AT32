#ifndef ELAB_TLE5012B_H
#define ELAB_TLE5012B_H

#include "elab_device.h"
#include "at32f403a_407.h" 

/* 1. TLE5012B 实例结构体 (继承自 elab_device_t) */
typedef struct {
    elab_device_t dev;      /* ⚠️ 必须是第一个成员，用于多态强转 */
    
    /* 硬件绑定参数 */
    spi_type *spi_x;        /* 绑定的 SPI 外设，如 SPI1 */
    gpio_type *cs_port;     /* CS 片选端口 */
    uint16_t cs_pin;        /* CS 片选引脚 */
    
    /* 运行数据缓存 */
    float current_angle;    /* 当前解析出的角度 (0.0 ~ 360.0度) */
} elab_tle5012b_t;


/* 2. 声明底层统一的注册入口函数 (实现在 .c 中) */
extern void elab_tle5012b_core_register(elab_tle5012b_t *inst, elab_device_attr_t *attr);
/* 3. 终极奥义：传感器自动注册宏 */
#define REGISTER_TLE5012B(_name_str, _spi, _cs_port, _cs_pin, _level) \
    static elab_tle5012b_t _inst_##_name_str = { \
        .spi_x = _spi, \
        .cs_port = _cs_port, \
        .cs_pin = _cs_pin \
    }; \
    static elab_device_attr_t _attr_##_name_str = { \
        .name = #_name_str, \
        .sole = true \
    }; \
    static void _init_##_name_str(void) { \
        elab_tle5012b_core_register(&_inst_##_name_str, &_attr_##_name_str); \
    } \
    ELAB_INIT_EXPORT(_init_##_name_str, _level)

#endif /* ELAB_TLE5012B_H */