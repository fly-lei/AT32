#include "elab_led_at32.h"
#include <stdbool.h>

/* enable 接口 */
static elab_err_t led_enable(elab_device_t *dev, bool status) {
    return ELAB_OK;
}

/* write 接口 */
static int led_write(elab_device_t *dev, uint32_t pos, const void *buffer, uint32_t size) {
    bool state = *(bool *)buffer;
    
    /* ✨ 魔法操作：因为 dev 是 elab_led_inst_t 的第一个成员，
     * 所以可以直接把基类指针“强转”回具体的实例指针，从而获取到硬件引脚！
     */
    elab_led_inst_t *led_inst = (elab_led_inst_t *)dev;
    
    /* 动态操作实例对应的 port 和 pin */
    gpio_bits_write(led_inst->port, led_inst->pin, state ? TRUE : FALSE);
    
    return size;
}

/* 统一的操作方法表 */
static const elab_dev_ops_t led_ops = {
    .enable = led_enable,
    .write  = led_write
};

/* 统一的底层注册入口 */
void elab_led_core_register(elab_led_inst_t *inst, elab_device_attr_t *attr) {
    /* 1. 绑定操作接口 */
    inst->dev.ops = &led_ops;
    
    /* 2. 调用 eLab 框架的注册接口 
     * 将 (elab_device_t *) 传进去是安全的，因为它是结构体的首地址
     */
    elab_device_register((elab_device_t *)inst, attr);
}