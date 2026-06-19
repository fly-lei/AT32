/**
 ******************************************************************************
 * @file    elab_usart1.c
 * @brief   USART1 极速异步驱动 (DMA 收发 + 空闲中断 + 硬件防死锁)
 ******************************************************************************
 */

#include "elab_usart1.h"
#include "at32f403a_407.h" // 确保包含雅特力底层库头文件
#include <stddef.h>

/* =================================================================================
 * 私有内部变量定义
 * ================================================================================= */
/* 双向完全解耦，上层不再需要 extern 这个数组，极致安全 */
static uint8_t g_rx_buffer[ELAB_USART1_RX_MAX_LEN] = {0};
static elab_usart_rx_callback_t g_rx_cb = NULL;

/* =================================================================================
 * 接口函数实现
 * ================================================================================= */

/**
 * @brief  初始化 USART1 及其 DMA 收发和空闲中断
 * @param  baudrate: 通讯波特率 (例如 115200)
 * @param  rx_callback: 接收完成一帧数据的回调函数
 */
void elab_usart1_init(uint32_t baudrate, elab_usart_rx_callback_t rx_callback)
{
    gpio_init_type gpio_init_struct;
    dma_init_type dma_init_struct;

    /* 1. 保存回调函数 */
    g_rx_cb = rx_callback;

    /* 2. 开启时钟：GPIOA, USART1, DMA1 */
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    /* 3. 配置 GPIO (PA9=TX, PA10=RX) */
    gpio_default_para_init(&gpio_init_struct);

    /* 配置 PA9 - USART1_TX (复用推挽输出) */
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_9;
    gpio_init(GPIOA, &gpio_init_struct);

    /* 配置 PA10 - USART1_RX (浮空输入) */
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_10;
    gpio_init(GPIOA, &gpio_init_struct);

    /* ========================================================= */
    /* 4. 配置 DMA1_CH5 (USART1_RX 接收通道)                     */
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

    dma_channel_enable(DMA1_CHANNEL5, TRUE); /* 接收 DMA 必须第一时间开启，准备接客 */

    /* ========================================================= */
    /* 5. 配置 DMA1_CH4 (USART1_TX 发送通道)                     */
    /* ========================================================= */
    dma_reset(DMA1_CHANNEL4);

    /* 🚀 致命修复 1：必须先清零结构体，洗掉 RX 通道残留的脏数据！ */
    dma_default_para_init(&dma_init_struct);

    dma_init_struct.buffer_size = 0; /* 发送时动态赋值 */
    dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_base_addr = 0; /* 发送时动态赋值 */

    /* 🚀 致命修复 2：必须明确告诉 DMA 往哪个外设地址塞数据！ */
    dma_init_struct.peripheral_base_addr = (uint32_t)&(USART1->dt);

    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_MEDIUM;
    dma_init_struct.loop_mode_enable = FALSE; /* 绝对不能开循环模式 */
    dma_init(DMA1_CHANNEL4, &dma_init_struct);

    dma_channel_enable(DMA1_CHANNEL4, FALSE); /* 初始必须关闭 */

    /* ========================================================= */
    /* 6. 配置 USART1 外设                                       */
    /* ========================================================= */
    usart_init(USART1, baudrate, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(USART1, TRUE);
    usart_receiver_enable(USART1, TRUE);

    /* 使能 USART1 的 DMA 收发请求 */
    usart_dma_transmitter_enable(USART1, TRUE);
    usart_dma_receiver_enable(USART1, TRUE);

    /* 7. 中断配置：只开启空闲中断 (IDLE) */
    usart_interrupt_enable(USART1, USART_IDLE_INT, TRUE);
    nvic_irq_enable(USART1_IRQn, 10, 0); /* 优先级设为 10，确保低于 FOC 电机中断，绝不抢占核心算法 */

    /* 最终使能 USART1 */
    usart_enable(USART1, TRUE);
}

/**
 * @brief  使用 DMA 极速非阻塞发送数据
 * @param  data: 待发送数据指针 (内存必须在全局区/静态区)
 * @param  len: 发送长度
 */
void elab_usart1_send_dma(uint8_t *data, uint16_t len)
{
    /* ==================================================== */
    /* 1. 防冲突保护与“硬件死锁自恢复”机制                  */
    /* ==================================================== */
    if (DMA1_CHANNEL4->ctrl_bit.chen == TRUE)
    {
        /* 🚨 如果 DMA 仍在开启状态，检查剩余搬运数量 */
        if (DMA1_CHANNEL4->dtcnt > 0)
        {
            /* 致命假死：DMA 在等，但串口不理它！(可能是总线被电机高频电磁干扰打断) */
            /* 【暴力自恢复】：强行关闭通道，复位串口 DMA 触发源！ */
            dma_channel_enable(DMA1_CHANNEL4, FALSE);
            usart_dma_transmitter_enable(USART1, FALSE);
            usart_dma_transmitter_enable(USART1, TRUE);
        }
        else
        {
            /* 正常发完了，但硬件 chen 标志偶发未拉低 (AT32 偶发小脾气) */
            dma_channel_enable(DMA1_CHANNEL4, FALSE);
        }
    }

    /* ==================================================== */
    /* 2. 标准的发射前彻底清空流程                          */
    /* ==================================================== */
    /* 必须显式关闭通道，才能改寄存器 */
    dma_channel_enable(DMA1_CHANNEL4, FALSE);

    /* 清除 DMA 通道 4 的全局标志位（包含完成、错误等一切暗雷） */
    dma_flag_clear(DMA1_GL4_FLAG);

    /* 🚀 关键修复：清除串口的“发送完成 (TDC)”标志，强行唤醒串口的 DMA 触发状态机！ */
    usart_flag_clear(USART1, USART_TDC_FLAG);

    /* ==================================================== */
    /* 3. 装填弹药并重新开火                                */
    /* ==================================================== */
    DMA1_CHANNEL4->maddr = (uint32_t)data;
    DMA1_CHANNEL4->dtcnt = len;

    dma_channel_enable(DMA1_CHANNEL4, TRUE);
}

/* =================================================================================
 * 中断服务函数
 * ================================================================================= */

/**
 * @brief  USART1 全局中断服务函数
 */
void USART1_IRQHandler(void)
{
    /* 检查是否是空闲中断 (接收完一帧数据) */
    if (usart_interrupt_flag_get(USART1, USART_IDLEF_FLAG) != RESET)
    {
        /* =============================================================== */
        /* 🚀 致命修复：清除 IDLE 中断标志位的唯一正确方法 (硬件机制)      */
        /* 必须先读 STS，再读 DT 寄存器，硬件才会自动清除该标志！          */
        /* =============================================================== */
        volatile uint32_t temp = USART1->sts;
        temp = USART1->dt;
        (void)temp; // 防止编译器报“变量未使用”的警告

        /* 2. 暂停接收 DMA 以锁定缓冲区，防止处理时被新数据覆盖 */
        dma_channel_enable(DMA1_CHANNEL5, FALSE);

        /* 3. 计算实际接收到了多少个字节 */
        uint16_t rx_len = ELAB_USART1_RX_MAX_LEN - DMA1_CHANNEL5->dtcnt;

        /* 4. 触发回调，将极速映射的数据抛给上层业务逻辑解析 */
        if (g_rx_cb != NULL && rx_len > 0)
        {
            g_rx_cb(g_rx_buffer, rx_len);
        }

        /* 5. 清理战场，重新装填接收 DMA，准备迎接下一帧数据 */
        dma_flag_clear(DMA1_GL5_FLAG); /* 🚀 补充细节：清除接收 DMA 的残留标志 */
        DMA1_CHANNEL5->dtcnt = ELAB_USART1_RX_MAX_LEN;
        dma_channel_enable(DMA1_CHANNEL5, TRUE);
    }
}