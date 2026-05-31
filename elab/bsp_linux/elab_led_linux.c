// elab/bsp_linux/elab_led_linux.c
#include "elab_device.h"
#include <stdio.h>
#include <stdbool.h>

static elab_device_t led_device;
static elab_err_t led_enable(elab_device_t *dev, bool status) {
    if (status) {
        printf("[INFO][LedLinux]: LED 硬件总线上电...\n");
    } else {
        printf("[INFO][LedLinux]: LED 硬件总线下电...\n");
    }
    return ELAB_OK;
}



/* Linux 下的伪装写操作：直接打印到终端 */
static int led_write(elab_device_t *dev, uint32_t pos, const void *buffer, uint32_t size) {
    bool state = *(bool *)buffer;
    // 用 emoji 模拟 LED 亮灭
    printf("💻 [Linux SIL] LED 状态: %s\n", state ? "🟢 亮起" : "⚫ 熄灭");
    return size;
}

/* ✨ 2. 把 enable 函数挂载到 ops 结构体上，填补 Line 189 的空缺！ */
static  const elab_dev_ops_t led_ops = {
    .enable = led_enable,  /* 填补空缺，消灭断言炸弹！ */
    .write  = led_write
};

// /* 在 Linux 的 main 中被调用 */
// void eLab_LED_Init(void) {
//     elab_device_register(&led_device, "led", &led_ops, NULL);
// }

 void led_hw_init(void) {
    /* 1. 绑定操作接口 */
    led_device.ops = &led_ops;
    
    /* 2. 构造属性 */
    static elab_device_attr_t attr = {
        .name = "led_status",
        .sole = false          /* 设为独占设备 */
    };
    
    /* 3. 调用 2 个参数的注册函数 */
    elab_device_register(&led_device, &attr);
}