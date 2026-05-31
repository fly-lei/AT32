/*
 * eLab Project
 * Copyright (c) 2026, EventOS Team, <event-os@outlook.com>
 * Generic Hardware Abstraction & Auto-Init Framework
 */

#ifndef ELAB_H
#define ELAB_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 聚合 eLab 核心组件 */
#include "elab_device.h"    /* VFS 设备层 (open, read, write) */
#include "elab_port.h"      /* 跨平台移植层 (断言, 临界区, 打印) */

/* ---------------------------------------------------------------------------*/
/* 核心 API 声明                                                                */
/* ---------------------------------------------------------------------------*/

/**
 * @brief  执行系统级多层自动化初始化。
 *         该函数会在 main() 早期被调用，它会遍历链接器收集到的各个 init 内存段，
 *         按照 Level 0 -> Level 3 的顺序，挨个执行底层硬件的初始化与设备注册。
 */
void eLab_InitAll(void);

/**
 * @brief  获取系统运行的毫秒级时间戳 (Tick)
 *         在 STM32 映射为 HAL_GetTick()，在 Linux 映射为 gettimeofday() 或类似实现
 */
uint32_t elab_time_ms(void);

/* ---------------------------------------------------------------------------*/
/* GCC Linker Magic (链接器魔法) : 自动注册宏定义                                  */
/* ---------------------------------------------------------------------------*/

/* 定义初始化函数指针的类型 */
typedef void (*elab_init_fn_t)(void);

/**
 * @brief  【自动初始化宏】
 *         使用 GCC 的 __attribute__((used, section(...))) 特性。
 *         这会让编译器把标记的函数指针，全部存放到指定的 Flash/RAM 内存段中。
 *         配合链接脚本 (.ld) 中的 KEEP 关键字，系统启动时只需遍历该内存段即可自动执行。
 * 
 * @param  fn     需要自动执行的无参初始化函数 (如 led_hw_init)
 * @param  level  初始化层级 (0: 框架级, 1: 核心外设, 2: 传感器扩展, 3: 业务组件)
 * 
 * @example
 *         static void bldc_motor_init(void) { ... }
 *         INIT_EXPORT(bldc_motor_init, 1);
 */
#define INIT_EXPORT(fn, level) \
    __attribute__((used, section(".elab_init_level_" #level))) \
    static const elab_init_fn_t __elab_init_##fn = fn

/* ---------------------------------------------------------------------------*/
/* 可选：轻量级轮询任务注册宏 (配合 eLab 的后台轮询引擎使用)                           */
/* 注：由于我们主推 QP/C 作为主引擎，POLL 宏通常用于不关心严格时序的低优先级杂项任务      */
/* ---------------------------------------------------------------------------*/

typedef void (*elab_poll_fn_t)(void);

typedef struct {
    elab_poll_fn_t fn;         /* 轮询执行的动作函数 */
    uint32_t       interval;   /* 执行周期 (毫秒) */
    uint32_t       last_tick;  /* 内部状态：上次执行的时间戳 */
} elab_poll_obj_t;

/**
 * @brief  【自动轮询任务宏】
 *         将任务挂载到 eLab 轮询引擎中。若使用了 QP/C，建议将其作为低级后备或禁用此功能。
 * 
 * @param  fn        任务函数
 * @param  interval  触发间隔 (ms)
 */
#define POLL_EXPORT(fn, interval) \
    __attribute__((used, section(".elab_poll_func"))) \
    elab_poll_obj_t __elab_poll_##fn = {fn, interval, 0}

#ifdef __cplusplus
}
#endif

#endif /* ELAB_H */