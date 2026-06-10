#include "elab_power_key.h"
#include <stddef.h>
/* 内部私有变量 */
static elab_key_callback_t g_key_evt_callback = NULL;
static uint8_t g_boot_lock = 1;      /* 开机锁：1-刚开机按键尚未释放 */
static uint16_t g_key_hold_cnt = 0;  /* 按键按下计数器 */
static uint8_t g_key_last_state = 0; /* 历史状态记录 */

/* 快捷物理操作宏 */
#define POWER_EN_HIGH() gpio_bits_set(ELAB_POWER_PORT, ELAB_POWER_PIN)
#define POWER_EN_LOW() gpio_bits_reset(ELAB_POWER_PORT, ELAB_POWER_PIN)
#define READ_KEY_PHYSICAL() (gpio_input_data_bit_read(ELAB_KEY_PORT, ELAB_KEY_PIN) == RESET)

/**
 * @brief  初始化电源控制与按键引脚，并注册事件回调
 * @param  callback: 上层应用的事件处理函数（如邮寄消息给活动对象）
 * @retval None
 */
void elab_power_key_init(elab_key_callback_t callback)
{

    /* 🚨 核心一步：开机瞬间拉高电源控制，锁住供电回路 */
    POWER_EN_HIGH();

    /* 4. 注册外部回调机制 */
    g_key_evt_callback = callback;

    /* 重置状态机内部变量 */
    g_boot_lock = 1;
    g_key_hold_cnt = 0;
    g_key_last_state = 0;
}

/**
 * @brief  按键扫描核心驱动（需要注册到 20ms 的定时器或系统 Tick 中）
 * @retval None
 */
void elab_power_key_tick_isr(void)
{
    /* 读取当前按键物理电平 (1: 按下, 0: 松开) */
    uint8_t key_curr_state = READ_KEY_PHYSICAL() ? 1 : 0;

    /* 机制 1：开机屏蔽锁死 */
    if (g_boot_lock)
    {
        if (key_curr_state == 0)
        {
            g_boot_lock = 0; /* 用户首次松开按键，安全解锁 */
        }
        g_key_last_state = key_curr_state;
        return; /* 未解锁前，不处理任何按键事件 */
    }

    /* 机制 2：长短按状态机扫描 */
    if (key_curr_state == 1)
    {
        g_key_hold_cnt++; /* 对应错误 2：修正笔误，改为全局定义的 g_key_hold_cnt */

        /* 判定长按：2秒阈值 (2000ms / 20ms = 100次) */
        if (g_key_hold_cnt == 100)
        {
            if (g_key_evt_callback != NULL)
            {
                g_key_evt_callback(ELAB_KEY_EVT_LONG_PRESS);
            }
        }
    }
    else
    {
        /* 按键松开瞬间检测 */
        if (g_key_last_state == 1)
        {
            /* 判定短按：大于40ms(防抖) 且 小于2秒 */
            if (g_key_hold_cnt > 2 && g_key_hold_cnt < 100)
            {
                if (g_key_evt_callback != NULL)
                {
                    g_key_evt_callback(ELAB_KEY_EVT_SHORT_PRESS);
                }
            }
            g_key_hold_cnt = 0; /* 清空计数 */
        }
    }

    g_key_last_state = key_curr_state;
}

/**
 * @brief  执行系统安全软关机（自尽回路）
 * @retval None
 */
void elab_system_shutdown(void)
{
    /* 进入临界区，关闭全局中断，防止断电瞬间产生硬件级毛刺和中断紊乱 */
    __disable_irq();
    /* 1. 关闭高频外设和总线，防止复位瞬间硬件状态机错乱 */
    dma_channel_enable(DMA1_CHANNEL5, FALSE);              // 关闭串口接收 DMA
    usart_interrupt_enable(USART1, USART_IDLE_INT, FALSE); // 关闭串口中断
    tmr_counter_enable(TMR1, FALSE);
    tmr_counter_enable(TMR8, FALSE); // 关闭 FOC 高级定时器 (极其重要，防止炸管)
    gpio_bits_reset(GPIOD, GPIO_PINS_10);
    gpio_bits_reset(GPIOE, GPIO_PINS_7);
    /* 彻底卸载硬件使能线，断开自锁回路 */
    // POWER_EN_LOW();

    /* 留在死循环中等待电容残余电量耗尽，软着陆 */
    while (1)
    {
        __NOP();
    }
}
void elab_system_poweron(void)
{

    // gpio_bits_set(GPIOD, GPIO_PINS_10);
    // gpio_bits_set(GPIOE, GPIO_PINS_7);
    /* 彻底卸载硬件使能线，断开自锁回路 */
}