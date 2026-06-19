#include "elab_usart1.h"
#include <stddef.h>

/* 私有内部变量：双向完全解耦，上层不再需要 extern 数组 */
static uint8_t g_rx_buffer[ELAB_USART1_RX_MAX_LEN] = {0};
static elab_usart_rx_callback_t g_rx_cb = NULL;

void elab_usart1_init(uint32_t baudrate, elab_usart_rx_callback_t rx_callback)
{
    gpio_init_type gpio_init_struct;
    dma_init_type dma_init_struct;

    /* 保存回调函数 */
    g_rx_cb = rx_callback;

    /* 1. 开启时钟：GPIOA, USART1, DMA1 */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    /* 2. 配置 GPIO (PA9=TX, PA10=RX) */
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_9;
    gpio_init(GPIOA, &gpio_init_struct);

    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_10;
    gpio_init(GPIOA, &gpio_init_struct);

    /* ========================================================= */
    /* 3. 配置 DMA1_CH5 (USART1_RX 接收通道)                     */
    /* ========================================================= */
    dma_reset(DMA1_CHANNEL5);
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.buffer_size = ELAB_USART1_RX_MAX_LEN;
    dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.memory_base_addr = (uint32_t)g_rx_buffer;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_base_addr = (uint32_t)&(USART1->dt);
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init_struct.loop_mode_enable = FALSE;
    dma_init(DMA1_CHANNEL5, &dma_init_struct);

    dma_channel_enable(DMA1_CHANNEL5, TRUE); /* 接收 DMA 必须第一时间开启 */

    /* ========================================================= */
    /* 4. 配置 DMA1_CH4 (USART1_TX 发送通道)                     */
    /* ========================================================= */
    dma_reset(DMA1_CHANNEL4);
    dma_init_struct.buffer_size = 0; /* 发送时动态赋值 */
    dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_base_addr = 0; /* 发送时动态赋值 */
    dma_init(DMA1_CHANNEL4, &dma_init_struct);

    dma_channel_enable(DMA1_CHANNEL4, FALSE); /* 🚨 发送 DMA 初始必须关闭 */

    /* 5. 配置 USART1 */
    usart_init(USART1, baudrate, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(USART1, TRUE);
    usart_receiver_enable(USART1, TRUE);

    /* 使能 USART1 的 DMA 请求 */
    usart_dma_transmitter_enable(USART1, TRUE);
    usart_dma_receiver_enable(USART1, TRUE);

    /* 6. 中断配置：只开启空闲中断 (IDLE) */
    usart_interrupt_enable(USART1, USART_IDLE_INT, TRUE);
    nvic_irq_enable(USART1_IRQn, 10, 0); /* 优先级设为 2，低于 FOC 电机中断 */

    usart_enable(USART1, TRUE);
}

/* ========================================================================= */
/* 普通轮询阻塞发送模式 (临时用于排查 DMA 问题，函数名保持不变以免上层报错)       */
/* ========================================================================= */

// void elab_usart1_send_dma(uint8_t *data, uint16_t len)
// {
//     /* 遍历待发送的每一个字节 */
//     for (uint16_t i = 0; i < len; i++)
//     {
//         /* 1. 等待发送数据寄存器为空 (TDBE: Transmit Data Buffer Empty)
//            如果不为空说明上一个字节还在排队，CPU 必须死等 */
//         while (usart_flag_get(USART1, USART_TDBE_FLAG) == RESET)
//         {
//             __NOP();
//         }

//         /* 2. 将当前字节塞入发送寄存器 */
//         usart_data_transmit(USART1, data[i]);
//     }

//     /* 3. 等待最后一个字节彻底从底层的移位寄存器中飞出去 (TDC: Transmit Data Complete) */
//     while (usart_flag_get(USART1, USART_TDC_FLAG) == RESET)
//     {
//         __NOP();
//     }
// }

void elab_usart1_send_dma(uint8_t *data, uint16_t len)
{
    /* 防冲突保护：如果 DMA 正在忙碌，直接退出 */
    if (DMA1_CHANNEL4->ctrl_bit.chen == TRUE)
        return;

    /* 🚀 必须显式关闭通道，才能修改地址和长度寄存器 */
    dma_channel_enable(DMA1_CHANNEL4, FALSE);

    /* 清除发送完成标志 */
    dma_flag_clear(DMA1_FDT4_FLAG);

    /* 装填新的内存地址和发送长度 */
    DMA1_CHANNEL4->maddr = (uint32_t)data;
    DMA1_CHANNEL4->dtcnt = len;

    /* 开火！ */
    dma_channel_enable(DMA1_CHANNEL4, TRUE);
}

void USART1_IRQHandler(void)
{
    /* 检查是否是空闲中断 */
    if (usart_interrupt_flag_get(USART1, USART_IDLEF_FLAG) != RESET)
    {
        /* =============================================================== */
        /* 🚀 致命修复：清除 IDLE 中断标志位的唯一正确方法 (硬件机制)      */
        /* 必须先读 STS，再读 DT 寄存器，硬件才会自动清除该标志！          */
        /* =============================================================== */
        volatile uint32_t temp = USART1->sts;
        temp = USART1->dt;
        (void)temp; // 防止编译器报“变量未使用”的警告

        /* 2. 暂停接收 DMA 以锁定缓冲区 */
        dma_channel_enable(DMA1_CHANNEL5, FALSE);

        /* 3. 计算实际接收到了多少个字节 */
        uint16_t rx_len = ELAB_USART1_RX_MAX_LEN - DMA1_CHANNEL5->dtcnt;

        /* 4. 触发回调，将数据抛给上层业务逻辑 */
        if (g_rx_cb != NULL && rx_len > 0)
        {
            g_rx_cb(g_rx_buffer, rx_len);
        }

        /* 5. 重新装填接收 DMA，准备迎接下一帧数据 */
        DMA1_CHANNEL5->dtcnt = ELAB_USART1_RX_MAX_LEN;
        dma_channel_enable(DMA1_CHANNEL5, TRUE);
    }
}

/* ========================================================================= */
/* 硬件中断服务函数 (接管串口空闲中断)                                       */
/* 注意：如果 at32f403a_407_int.c 中已经有了此函数，请将那边的删掉以免冲突 */
/* ========================================================================= */

// void USART1_IRQHandler(void)
// {
//     /* add user code begin USART1_IRQ 0 */

//     /* add user code end USART1_IRQ 0 */

//     if (usart_interrupt_flag_get(USART1, USART_IDLEF_FLAG) != RESET)
//     {
//         /* add user code begin USART1_USART_IDLEF_FLAG */
//         /* clear flag */
//         usart_flag_clear(USART1, USART_IDLEF_FLAG);
//         //         /* 2. 暂停接收 DMA 以锁定缓冲区 */
//         dma_channel_enable(DMA1_CHANNEL5, FALSE);

//         /* 3. 计算实际接收到了多少个字节
//            总长度 - DMA 剩余未搬运的计数 = 真实接收长度 */
//         uint16_t rx_len = ELAB_USART1_RX_MAX_LEN - DMA1_CHANNEL5->dtcnt;

//         /* 4. 触发回调，将数据抛给上层业务逻辑 */
//         if (g_rx_cb != NULL && rx_len > 0)
//         {
//             g_rx_cb(g_rx_buffer, rx_len);
//         }

//         /* 5. 重新装填接收 DMA，准备迎接下一帧数据 */
//         DMA1_CHANNEL5->dtcnt = ELAB_USART1_RX_MAX_LEN;
//         dma_channel_enable(DMA1_CHANNEL5, TRUE);
//         /* add user code end USART1_USART_IDLEF_FLAG */
//     }

//     /* add user code begin USART1_IRQ 1 */

//     /* add user code end USART1_IRQ 1 */
// }