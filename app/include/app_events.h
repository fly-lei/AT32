/*
 * eLab Project
 * Copyright (c) 2026, EventOS Team, <event-os@outlook.com>
 * Application Level Events & Signals Definition
 */

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "qpc.h" /* 引入 QP/C 核心框架 */
#include <stdint.h>
#include <stdbool.h>

    /* ===========================================================================*/
    /* 1. 全局信号枚举 (Signals Dictionary)                                       */
    /* ===========================================================================*/
    /**
     * @brief 系统所有的自定义信号必须从 Q_USER_SIG 开始排号。
     * 建议按功能模块分组，方便在 QSPY 追踪时一目了然。
     */
    enum AppSignals
    {
        /* --- 时间节拍信号 (Time Events) --- */
        TICK_5MS_SIG = Q_USER_SIG, /* 高频控制节拍：触发 LQR/PID 姿态解算 */
        TICK_50MS_SIG,             /* 中频节拍：用于遥控接收心跳、UI 刷新 */
        TICK_500MS_SIG,            /* 低频节拍：电池电量监控、状态指示灯闪烁 */
        START_SIG,
        TIMEOUT_SIG,
        STOP_CMD_SIG,
        /* --- 遥控与指令信号 (Control Commands) --- */
        CMD_START_BALANCE_SIG,   /* 指令：从待机进入平衡模式 */
        CMD_STOP_BALANCE_SIG,    /* 指令：进入待机/卸载电机力矩 */
        CMD_UPDATE_SETPOINT_SIG, /* 指令：更新期望的速度和转向角 */

        /* --- 传感器数据就绪信号 (Sensor Updates) --- */
        IMU_DATA_READY_SIG, /* 硬件中断抛出：新的一帧 IMU 数据已解析 */
        ENCODER_UPDATE_SIG, /* 编码器测速更新 */

        /* --- 严重故障与安全信号 (Faults & Safety) --- */
        FAULT_FALLEN_SIG,      /* 物理保命：倾角过大，判定为跌倒！必须立刻切断动力 */
        FAULT_OVERCURRENT_SIG, /* 物理保命：底层 FOC 报电机过流 */
        FAULT_LOW_BATTERY_SIG, /* 电池电压过低警告 */

        /* 信号总数上限，用于定义发布-订阅 (Publish-Subscribe) 列表的大小 */
        MAX_PUB_SIG
    };

    /* ===========================================================================*/
    /* 2. 事件载荷结构体 (Event Structures with Payload)                            */
    /* ===========================================================================*/
    /**
     * 💡 QP/C 架构铁律：
     * 任何自定义的事件结构体，它的第一个成员【必须】是 `QEvt super;`
     * 这是 C 语言实现单继承的标准做法，使得子类指针可以安全转型为 QEvt 基类指针。
     */

    /**
     * @brief 遥控/期望目标设定事件
     * 当收到蓝牙串口或按键的控制包时，Comm_AO 将打包此事件发送给 Control_AO
     */
    typedef struct
    {
        QEvt super; /* 继承 QP 事件基类 (强制要求) */

        float target_speed;     /* 期望的前进速度 (m/s) */
        float target_turn_rate; /* 期望的转向角速度 (rad/s) */
    } CmdUpdateEvt;

    /**
     * @brief 传感器聚合数据事件
     * 用于解耦“数据采集任务”与“姿态控制任务”
     */
    typedef struct
    {
        QEvt super;

        float pitch_angle; /* 经滤波后的当前俯仰角 (rad) */
        float pitch_rate;  /* 陀螺仪原始角速度 (rad/s) */
        float wheel_speed; /* 车轮线速度反馈 (m/s) */
    } SensorDataEvt;

    /**
     * @brief 故障参数事件
     * 传递发生故障时的具体物理量，供日志打印和黑匣子分析
     */
    typedef struct
    {
        QEvt super;

        float fault_value;     /* 发生故障时的具体数值（如跌倒时的 65.5 度，过流时的 12.3A） */
        uint32_t timestamp_ms; /* 故障发生的 eLab 系统时间戳 */
    } FaultEvt;

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENTS_H */