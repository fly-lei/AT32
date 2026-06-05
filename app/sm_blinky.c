// app/sm_blinky.c
#include "qpc.h"
#include "elab_device.h"
#include <stdbool.h>
#include <stdio.h>
#include "app_events.h"
#include "elab_foc_motor.h"
#include "elab_adc3.h"
// 声明外部指针 (你在 System_Link_Devices 里找好的)
extern elab_foc_motor_t *g_motor_L;
extern elab_foc_motor_t *g_motor_R;
Q_DEFINE_THIS_MODULE("sm_blinky")

/* 1. 定义 Blinky 状态机对象 */
typedef struct
{
    QActive super;
    QTimeEvt timeEvt;   // 定时器事件
    QTimeEvt STOPTIMER; // 定时器事件
    QTimeEvt te_telemetry;
    bool led_state; // 内部状态
    float angle_L;
    float angle_R;
    elab_device_t *led_dev; // 虚拟设备句柄
    elab_device_t *imu_sensor;

} Blinky;

// 获取温度的伪代码（留给你补充 NTC 曲线）
uint16_t Get_Temp1_Raw(void) { return g_adc3_buffer[1]; }
uint16_t Get_Temp2_Raw(void) { return g_adc3_buffer[2]; }

static Blinky l_blinky; // 静态实例

QActive *const AO_Blinky = &l_blinky.super;

/* 状态函数前置声明 */
static QState Blinky_initial(Blinky *const me, void const *const par);
static QState Blinky_active(Blinky *const me, QEvt const *const e);

/* 2. 状态机启动与构造 */
void Blinky_ctor(void)
{
    Blinky *me = &l_blinky;
    QActive_ctor(&me->super, Q_STATE_CAST(&Blinky_initial));
    QTimeEvt_ctorX(&me->timeEvt, &me->super, TIMEOUT_SIG, 0U);         // 信号 TIMEOUT_SIG
    QTimeEvt_ctorX(&me->STOPTIMER, &me->super, STOP_SHUTDOWN_SIG, 0U); // 信号 STOP_SHUTDOWN_SIG
                                                                       /* 构造时间事件，绑定到 TELEMETRY_TICK_SIG 信号 */
    QTimeEvt_ctorX(&me->te_telemetry, (QActive *)me, TELEMETRY_TICK_SIG, 0U);
}

/* 3. 初始伪状态 (只执行一次) */
static QState Blinky_initial(Blinky *const me, void const *const par)
{
    (void)par;

    /* 🚀 上发条：延时 20 个 Tick 后首次触发，之后每 20 个 Tick 循环触发 */
    QTimeEvt_armX(&me->te_telemetry, 20U, 20U);
    // 寻找 VFS 设备节点
    me->led_dev = elab_device_find("led_status");
    // me->imu_sensor = elab_device_find("imu_sensor");
    if (me->led_dev)
        elab_device_open(me->led_dev);
    static QEvt const startEvt = QEVT_INITIALIZER(START_SIG);
    QACTIVE_POST(&me->super, &startEvt, me);
    return Q_TRAN(&Blinky_active);
    // // 启动周期性定时器 (假设 系统节拍是 1ms，这里定时 500ms)
    // QTimeEvt_armX(&me->timeEvt, 500U, 500U);
    // return Q_TRAN(&Blinky_active);
}

/* 4. 活动状态 (循环处理事件) */
static QState Blinky_active(Blinky *const me, QEvt const *const e)
{
    switch (e->sig)
    {
    case Q_ENTRY_SIG:
        /* 让左轮以 0.05 的 Vq (轻微扭矩) 往前转 */
        if (g_motor_L && g_motor_R)
        {
            // g_motor_L->run_foc.Cali_flag = 1; // 强制触发一次寻零校准 (约2秒)
            // g_motor_L->target_Vq = 0.0f;      // 校准完毕切入 case 1 后，按照 0.05f 运转！
            // g_motor_R->run_foc.Cali_flag = 1; // 强制触发一次寻零校准 (约2秒)
            // g_motor_R->target_Vq = 0.0f;     // 校准完毕切入 case 1 后，按照 0.05f 运转！
        }

        return Q_HANDLED();

    case TELEMETRY_TICK_SIG:
        /* 该函数内部会收集最新数据，并启用 DMA 瞬间发送完毕 */
        Protocol_Send_Telemetry();
        return Q_HANDLED();
        
    case STOP_CMD_SIG: // 收到停止指令
        if (g_motor_L)
        {
            g_motor_L->target_Vq = 0.0f; // 扭矩归零，自由滑行
            // 或者直接断电：g_motor_L->run_foc.Cali_flag = 0;
        }
        if (g_motor_R)
        {
            g_motor_R->target_Vq = 0.0f; // 扭矩归零，自由滑行
            // 或者直接断电：g_motor_R->run_foc.Cali_flag = 0;
        }
        QTimeEvt_armX(&me->STOPTIMER, 500U, 0U); // 启动一次性定时器，500ms 后触发 STOP_SHUTDOWN_SIG

        return Q_HANDLED();

    case START_SIG:
    {
        /* 此时 QF_run() 已经开始运行，时间引擎已就绪，安全启动定时器！ */
        QTimeEvt_armX(&me->timeEvt, 500U, 500U);

        return Q_HANDLED();
    }
    case STOP_SHUTDOWN_SIG:
    {
        elab_system_shutdown();

        return Q_HANDLED();
    }
    case SHORT_PRESS_SIG:
    {
        elab_system_poweron();
        if (g_motor_L && g_motor_R)
        {
            g_motor_L->run_foc.Cali_flag = 1; // 强制触发一次寻零校准 (约2秒)
            g_motor_L->target_Vq = 0.1f;      // 校准完毕切入 case 1 后，按照 0.05f 运转！
            g_motor_R->run_foc.Cali_flag = 1; // 强制触发一次寻零校准 (约2秒)
            g_motor_R->target_Vq = 0.1f;      // 校准完毕切入 case 1 后，按照 0.05f 运转！
        }
        return Q_HANDLED();
    }
    case LONG_PRESS_SIG:
    {

        static QEvt const stop_cmd = QEVT_INITIALIZER(STOP_CMD_SIG);

        /* 安全投递，第三个参数填 0U (无 QP Spy 追踪) 即可 */
        QACTIVE_POST(AO_Blinky, &stop_cmd, 0U);

        return Q_HANDLED();
    }

    case TIMEOUT_SIG:
    {                                   // 定时器滴答事件到达
        me->led_state = !me->led_state; // 逻辑翻转
                                        //         if (me->enc_left) {
                                        //             elab_device_read(me->enc_left, 0, &me->angle_L, sizeof(float));
                                        //         }
                                        //         if (me->enc_right) {
                                        //             elab_device_read(me->enc_right, 0, &me->angle_R, sizeof(float));
                                        //         }
                                        //        float imu_data[6]; // 用于存放 3轴加速度 + 3轴陀螺仪
                                        //        int read_bytes = elab_device_read(me->imu_sensor, 0, imu_data, sizeof(imu_data));

        //     /* 3. 数据处理 */
        //         if (read_bytes == sizeof(imu_data)) {

        // }
        uint8_t battery_percent = Get_Battery_Percent(Get_Battery_Voltage());
        Get_Motor_Temperature(); // 直接调用获取温度的函数，虽然现在没存储结果，但你可以在这里添加日志输出或其他处理
        if (me->led_dev)
        {
            // 将布尔值通过 VFS 写入底层
            elab_device_write(me->led_dev, 0, &me->led_state, sizeof(bool));
        }
        return Q_HANDLED();
    }
    }
    return Q_SUPER(&QHsm_top);
}