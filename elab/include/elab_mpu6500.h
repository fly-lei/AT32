/* elab_mpu6500.h */


#include "elab_device.h"
#include "at32f403a_407.h" 

/* 1. MPU6500 设备实例结构体 */
typedef struct {
    elab_device_t dev;      /* ⚠️ 永远在第一位，继承基础设备大类 */
    
    /* SPI 绑定信息 */
    spi_type *spi_x;        /* 绑定的 SPI 外设，例如 SPI1 */
    gpio_type *cs_port;     /* CS (NSS) 片选端口，例如 GPIOA */
    uint16_t cs_pin;        /* CS 片选引脚，例如 GPIO_PINS_4 */
    
    /* 传感器状态缓冲 */
    uint8_t who_am_i;       /* 传感器 ID */
    float accel[3];         /* 三轴加速度缓冲 */
    float gyro[3];          /* 三轴陀螺仪缓冲 */
} elab_mpu6500_t;

/* 2. 声明底层注册函数 */
extern void elab_mpu6500_core_register(elab_mpu6500_t *inst, elab_device_attr_t *attr);
/* 3. MPU6500 自动注册宏 */
#define REGISTER_MPU6500_SPI(_name_str, _spi, _cs_port, _cs_pin, _level) \
    static elab_mpu6500_t _inst_##_name_str = { \
        .spi_x = _spi, \
        .cs_port = _cs_port, \
        .cs_pin = _cs_pin \
    }; \
    static elab_device_attr_t _attr_##_name_str = { \
        .name = #_name_str, \
        .sole = false /* 传感器通常是独占设备 */ \
    }; \
    static void _init_##_name_str(void) { \
        elab_mpu6500_core_register(&_inst_##_name_str, &_attr_##_name_str); \
    } \
    ELAB_INIT_EXPORT(_init_##_name_str, _level)