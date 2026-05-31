/*
 * elab/bsp_linux/elab_port_linux.c
 */
#include "elab_port.h"
#include "elab.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

/* 1. 时基对齐 */
uint32_t elab_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* 2. 仿真层崩溃处理 */
void System_Panic(char const * const file, int const line) {
    fprintf(stderr, "\n❌ [SIL PANIC] Simulation Aborted!\n");
    fprintf(stderr, "File: %s , Line: %d\n", file, line);
    exit(EXIT_FAILURE);
}

// /* 兼容不同版本 QP/C 的断言接口 */
// void Q_onAssert(char const * const module, int_t const location) {
//     System_Panic(module, location);
// }

void Q_onError(char const * const module, int_t const location) {
    System_Panic(module, location);
}

/* 3. 日志美化 */
void elab_port_log(uint8_t level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const char* colors[] = {"\033[36m", "\033[32m", "\033[33m", "\033[31m"};
    const char* level_strs[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    
    printf("%s[%s][%s]: ", colors[level], level_strs[level], tag);
    vprintf(format, args);
    printf("\033[0m\n");
    
    va_end(args);
}