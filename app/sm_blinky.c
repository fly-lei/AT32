// app/sm_blinky.c
#include "qpc.h"
#include "elab_device.h"
#include <stdbool.h>
#include <stdio.h>
#include "app_events.h"
#include "elab_foc_motor.h"

// 声明外部指针 (你在 System_Link_Devices 里找好的)
extern elab_foc_motor_t *g_motor_L;
extern elab_foc_motor_t *g_motor_R;
Q_DEFINE_THIS_MODULE("sm_blinky")

/* 1. 定义 Blinky 状态机对象 */
typedef struct
{
    QActive super;
    QTimeEvt timeEvt; // 定时器事件
    bool led_state;   // 内部状态
    float angle_L;
    float angle_R;
    elab_device_t *led_dev; // 虚拟设备句柄
    elab_device_t *imu_sensor;
    elab_device_t *enc_left;
    elab_device_t *enc_right;
} Blinky;

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
    QTimeEvt_ctorX(&me->timeEvt, &me->super, TIMEOUT_SIG, 0U); // 信号 TIMEOUT_SIG
}

/* 3. 初始伪状态 (只执行一次) */
static QState Blinky_initial(Blinky *const me, void const *const par)
{
    (void)par;
    // 寻找 VFS 设备节点
    me->led_dev = elab_device_find("led_status");
    // me->imu_sensor = elab_device_find("imu_sensor");
    // me->enc_left = elab_device_find("enc_left");
    // me->enc_right = elab_device_find("enc_right");
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
    /* 🔴 探针 2：看状态机是否真的收到了事件 */
    // printf(">>> DEBUG [AO]: Blinky_active received SIG = %d\n", e->sig);
    switch (e->sig)
    {
    case Q_ENTRY_SIG:
        /* 让左轮以 0.05 的 Vq (轻微扭矩) 往前转 */
        if (g_motor_L)
        {
            g_motor_L->run_foc.Cali_flag = 3; // 强制触发一次寻零校准 (约2秒)
            g_motor_L->target_Vq = 0.15f;     // 校准完毕切入 case 1 后，按照 0.05f 运转！
        }
        return Q_HANDLED();

    case STOP_CMD_SIG: // 收到停止指令
        if (g_motor_L)
        {
            g_motor_L->target_Vq = 0.0f; // 扭矩归零，自由滑行
            // 或者直接断电：g_motor_L->run_foc.Cali_flag = 0;
        }
        return Q_HANDLED();

    case START_SIG:
    {
        /* 此时 QF_run() 已经开始运行，时间引擎已就绪，安全启动定时器！ */
        QTimeEvt_armX(&me->timeEvt, 500U, 500U);

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