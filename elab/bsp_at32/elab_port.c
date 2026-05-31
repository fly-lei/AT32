/*
 * elab/bsp_stm32/elab_port.c (仅用于 STM32 目标)
 */
#include "elab_port.h"
#include "elab.h"
#include <stdio.h>
#include <stdarg.h>

#if !defined(__linux__)
#include "elab_led_at32.h"

/* ---------------------------------------------------------------------------*/
/* 1. 时基对齐                                                               */
/* ---------------------------------------------------------------------------*/
uint32_t elab_time_ms(void) {
    return HAL_GetTick(); /* 映射到硬件 SysTick 的全局毫秒计数器 */
}

/* ---------------------------------------------------------------------------*/
/* 2. 物理层致命错误兜底 (System Panic)                                      */
/* ---------------------------------------------------------------------------*/
void System_Panic(char const * const file, int const line) {
    /* 1. 核心物理安全：直接操作外设寄存器，全速切断高级定时器的 PWM 主输出 (MOE) */
    /* 强行切断 BLDC/电机动力，防止机械结构暴走 */
    #ifdef TIM1
    TIM1->BDTR &= ~TIM_BDTR_MOE; 
    #endif
    #ifdef TIM8
    TIM8->BDTR &= ~TIM_BDTR_MOE;
    #endif

    /* 2. 关闭全局中断，冻结当前所有任务时序 */
    __disable_irq();

    /* 3. 打印死亡遗言 */
    printf("\r\n💥 [CRITICAL PANIC] Hardware Halted!\r\n");
    printf("Location: %s : Line %d\r\n", file, line);

    /* 4. 原地死循环，保持电平安全，等待 VS Code 按下暂停查看堆栈 */
    while (1) {
        /* 可在此处加入低级 GPIO 操作让 LED 闪烁报错 */
    }
}

/* QP/C 框架内置断言回调，直接汇流到统一死亡处理中心 */
void Q_onAssert(char const * const module, int_t const location) {
    System_Panic(module, location);
}

/* ---------------------------------------------------------------------------*/
/* 3. 硬件端多通道日志分发                                                    */
/* ---------------------------------------------------------------------------*/
void elab_port_log(uint8_t level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);

#ifdef Q_SPY
    /* 如果开启了 Q_SPY 追踪，文本将自动打包进二进制流，避免传统串口打架 */
    // QP/C 专属二进制字节流输出逻辑...
#else
    /* 正常未加锁模式下，增加控制台彩色打印前缀，直接重定向输出到标准串口 */
    const char* levels[] = {"[D]", "[I]", "[W]", "[E]"};
    printf("%s[%s]: ", levels[level], tag);
    vprintf(format, args);
    printf("\r\n");
#endif

    va_end(args);
}

#endif /* !__linux__ */