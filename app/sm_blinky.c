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

} Blinky;

/* 定义 2S 动力电池的真实放电曲线 (必须按电压从高到低排列) */
typedef struct
{
    float voltage;
    uint8_t soc; // 0-100%
} BatteryCurve_t;

const BatteryCurve_t bat_curve[] = {
    {8.40f, 100},
    {8.10f, 90},
    {7.96f, 80},
    {7.84f, 70},
    {7.68f, 60},
    {7.52f, 50},
    {7.40f, 40},
    {7.28f, 30},
    {7.14f, 20},
    {6.90f, 10},
    {6.60f, 5},
    {6.00f, 0} // 平衡车强负载下，截止电压可设低至 6.0V 防止急加速断电
};
#define CURVE_POINTS (sizeof(bat_curve) / sizeof(bat_curve[0]))

/* 将测得的实际电压转换为 0~100% */
uint8_t Get_Battery_Percent(float real_vol)
{
    /* 1. 极值保护 */
    if (real_vol >= bat_curve[0].voltage)
        return 100;
    if (real_vol <= bat_curve[CURVE_POINTS - 1].voltage)
        return 0;

    /* 2. 遍历查找表，找到电压落在哪两个点之间 */
    for (int i = 0; i < CURVE_POINTS - 1; i++)
    {
        if (real_vol < bat_curve[i].voltage && real_vol >= bat_curve[i + 1].voltage)
        {
            /* 3. 在这两个点之间进行线性插值，使电量过渡平滑 */
            float v_diff = bat_curve[i].voltage - bat_curve[i + 1].voltage;
            float p_diff = (float)(bat_curve[i].soc - bat_curve[i + 1].soc);
            float v_offset = real_vol - bat_curve[i + 1].voltage;

            return bat_curve[i + 1].soc + (uint8_t)((v_offset / v_diff) * p_diff);
        }
    }
    return 0;
}
/* 假设你的电池是通过 10K 和 1K 电阻分压的 (分压比 11:1) */
#define VOLTAGE_DIVIDER_RATIO 11.0f
#define ADC_REF_VOLTAGE 3.3f
extern uint16_t g_adc3_buffer[3]; // 定义全局 DMA 缓冲区

/* NTC 物理参数宏定义 */
#define NTC_R25 10000.0f    // 25度时的阻值 10k
#define NTC_B_VALUE 3380.0f // B值 3380K
#define R_DIVIDER 3000.0f   // 下方的分压电阻 3k

/* 获取真实的电机温度 (摄氏度) */
float Get_Motor_Temperature(void)
{
    /* 1. 从 DMA 数组读取原始 ADC 值 (假设 12位 ADC，最大 4095) */
    float adc_val = (float)g_adc3_buffer[1];

    /* 防除零保护 (拔掉传感器或者短路时) */
    if (adc_val < 10.0f || adc_val > 4085.0f)
    {
        return -99.0f; // 传感器异常标志
    }

    /* 2. 硬件分压逆推 NTC 当前阻值
     * 公式: V_adc = 3.3 * (3000 / (R_ntc + 3000))
     * 化简得到 R_ntc 的极速算法：
     */
    float r_ntc = R_DIVIDER * (4095.0f - adc_val) / adc_val;

    /* 3. 使用标准 B值公式计算开尔文温度 (Kelvin)
     * T = 1 / ( 1/T25 + (1/B)*ln(Rt/R25) )
     */
    float kelvin = 1.0f / ((1.0f / 298.15f) + (1.0f / NTC_B_VALUE) * logf(r_ntc / NTC_R25));

    /* 4. 开尔文转摄氏度 */
    float celsius = kelvin - 273.15f;

    return celsius;
}
float Get_Battery_Voltage(void)
{
    /* 直接从 DMA 数组的第 0 个元素拿原始数据 */
    uint16_t raw_vol = g_adc3_buffer[0];

    /* 换算公式：(原始值 / 4095) * 3.3V * 分压比 */
    float real_vol = ((float)raw_vol / 4095.0f) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER_RATIO;
    return real_vol;
}

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
    QTimeEvt_ctorX(&me->timeEvt, &me->super, TIMEOUT_SIG, 0U); // 信号 TIMEOUT_SIG
}

/* 3. 初始伪状态 (只执行一次) */
static QState Blinky_initial(Blinky *const me, void const *const par)
{
    (void)par;
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
            g_motor_L->run_foc.Cali_flag = 1; // 强制触发一次寻零校准 (约2秒)
            g_motor_L->target_Vq = 0.1f;      // 校准完毕切入 case 1 后，按照 0.05f 运转！
            g_motor_R->run_foc.Cali_flag = 1; // 强制触发一次寻零校准 (约2秒)
            g_motor_R->target_Vq = -0.1f;     // 校准完毕切入 case 1 后，按照 0.05f 运转！
        }

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