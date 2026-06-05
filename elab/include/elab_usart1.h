#ifndef __ELAB_USART1_H
#define __ELAB_USART1_H

#include "at32f403a_407.h"
#include <stdint.h>

/* 定义单帧最大接收长度 (依据你的二进制协议，9字节其实很小，64足够防溢出) */
#define ELAB_USART1_RX_MAX_LEN 64

/* ========================================================================= */
/* 类型定义：接收完成回调函数指针                                            */
/* 当底层发生串口空闲中断时，将自动调用此函数，并交出数据首地址和有效长度    */
/* ========================================================================= */
typedef void (*elab_usart_rx_callback_t)(uint8_t *data, uint16_t len);

/* ========================================================================= */
/* 公共 API                                                                  */
/* ========================================================================= */

/**
 * @brief  初始化 USART1 及其收发 DMA 通道，并注册接收回调
 * @param  baudrate: 波特率 (如 115200, 460800)
 * @param  rx_callback: 接收到一帧完整数据后的处理函数
 */
void elab_usart1_init(uint32_t baudrate, elab_usart_rx_callback_t rx_callback);

/**
 * @brief  使用 DMA 非阻塞发送数据
 * @param  data: 待发送数据的首地址
 * @param  len: 字节长度
 */
void elab_usart1_send_dma(uint8_t *data, uint16_t len);

#endif /* __ELAB_USART1_H */