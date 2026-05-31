/*
 * eLab Project
 * Copyright (c) 2026, EventOS Team, <event-os@outlook.com>
 * Cross-Platform Porting Layer for STM32 and Linux SIL
 */

#ifndef ELAB_PORT_H
#define ELAB_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "qpc.h"  /* 🧠 引入 QP/C 核心，确保临界区和断言的绝对对齐 */

/* ---------------------------------------------------------------------------*/
/* 1. 临界区保护 (Critical Sections) - 与 QP/C 统一护盾                       */
/* ---------------------------------------------------------------------------*/
/* * 放弃粗暴的 __disable_irq()，全面征用 QP/C 的全功能嵌套临界区。
 * 无论是在 Linux 的 POSIX 互斥锁还是 STM32 的内核屏蔽字下，均由 QP 统筹，绝不挂起线程。
 */
#define ELAB_CRITICAL_ENTER()   QF_CRIT_ENTRY()
#define ELAB_CRITICAL_EXIT()    QF_CRIT_EXIT()

/* ---------------------------------------------------------------------------*/
/* 2. 断言与异常处理 (Assert & Panic)                                         */
/* ---------------------------------------------------------------------------*/
void System_Panic(char const * const file, int const line);

#define elab_assert(expr) \
    do { \
        if (!(expr)) { \
            System_Panic(__FILE__, __LINE__); \
        } \
    } while (0)

#define elab_assert_name(expr, name)  elab_assert(expr)

/* ---------------------------------------------------------------------------*/
/* 3. 高级日志打印系统 (Log System)                                           */
/* ---------------------------------------------------------------------------*/
/* 定义日志级别 */
#define ELAB_LOG_DEBUG  0
#define ELAB_LOG_INFO   1
#define ELAB_LOG_WARN   2
#define ELAB_LOG_ERROR  3

void elab_port_log(uint8_t level, const char *tag, const char *format, ...);

/* 定义局部文件日志标签，配合源文件中的 ELAB_TAG("...") 使用 */
#ifndef ELAB_TAG
#define ELAB_TAG "GLOBAL"
#endif

#define ELAB_LOG_D(format, ...)  elab_port_log(ELAB_LOG_DEBUG, ELAB_TAG, format, ##__VA_ARGS__)
#define ELAB_LOG_I(format, ...)  elab_port_log(ELAB_LOG_INFO,  ELAB_TAG, format, ##__VA_ARGS__)
#define ELAB_LOG_W(format, ...)  elab_port_log(ELAB_LOG_WARN,  ELAB_TAG, format, ##__VA_ARGS__)
#define ELAB_LOG_E(format, ...)  elab_port_log(ELAB_LOG_ERROR, ELAB_TAG, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* ELAB_PORT_H */